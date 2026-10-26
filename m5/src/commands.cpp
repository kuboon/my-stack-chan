// The recognizer follows KoeAct-JP's CoreS3 firmware (cores3/firmware/e05/main/main.cpp,
// Apache-2.0, Copyright 2026 ayousanz / ayutaz; see lib/koeact/NOTICE): the stream .espdl takes
// 40 log-Mel frames (0.4 s) at a time with its state carried over, and each chunk's layer4 /
// layer8 outputs are encoded into the a2 decoder right away, so only the last chunk and the
// greedy decoding are left when the user stops speaking.
#include "commands.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include <esp_heap_caps.h>
#include <esp_partition.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include "a2.h"
#include "action_pieces.h"
#include "dl_model_base.hpp"
#include "features.h"

namespace commands {
namespace {

constexpr int kBins = 40;
constexpr int kSubsampling = 4;
constexpr int kMaxFrames = 1500;                    // 15 s
constexpr int kMaxSteps = kMaxFrames / kSubsampling;  // a2's capacity
/// KoeAct-JP's config.json: below this p_act the answer is "[]".
constexpr float kThreshold = 0.97f;
/// As in training (koeact.runtime.clean): 0.5 s of silence before the speech, 0.3 s after.
constexpr int kLeadSamples = ralomi::kSampleRate / 2;
constexpr int kTailSamples = ralomi::kSampleRate * 3 / 10;
constexpr int kMaxSamples = (kMaxFrames - 1) * ralomi::kHop + ralomi::kWindow;

const char* const kVocabulary[] = {
#include "vocabulary.inc"
};
constexpr int kVocabularySize = sizeof kVocabulary / sizeof kVocabulary[0];

dl::Model* model = nullptr;
dl::TensorBase* featuresIn = nullptr;
dl::TensorBase* logits = nullptr;
dl::TensorBase* layer4 = nullptr;
dl::TensorBase* layer8 = nullptr;
int chunkFrames = 0;

struct State {
    dl::TensorBase* input;
    dl::TensorBase* output;
    std::vector<uint8_t> saved;
};
std::vector<State> states;

a2_model decoder;
void* arena = nullptr;  // PSRAM
float* chunkIn = nullptr;  // PSRAM, one chunk of decoder frames

float* pcm = nullptr;       // PSRAM, the utterance as float in [-1, 1]
float* features = nullptr;  // PSRAM

// Shared with the main loop.
volatile int written = 0;        // samples in pcm
volatile bool finished = false;  // no more samples will come
volatile bool running = false;   // between start() and the result
volatile uint32_t session = 0;
volatile uint32_t endedAt = 0;   // millis() at finish()
SemaphoreHandle_t lock = nullptr;  // guards pending and hasPending (Strings: no spinlock)
Result pending;
bool hasPending = false;
TaskHandle_t task = nullptr;
bool loaded = false;

bool partitionStartsWith(const char* label, const char* magic, size_t n) {
    const esp_partition_t* p =
        esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, label);
    if (!p) return false;
    char head[8] = {};
    if (esp_partition_read(p, 0, head, n) != ESP_OK) return false;
    return memcmp(head, magic, n) == 0;
}

bool loadDecoder() {
    const esp_partition_t* part =
        esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "a2");
    if (!part) return false;
    const void* map = nullptr;
    esp_partition_mmap_handle_t handle;
    if (esp_partition_mmap(part, 0, part->size, ESP_PARTITION_MMAP_DATA, &map, &handle) != ESP_OK)
        return false;
    uint32_t payload = 0;
    memcpy(&payload, static_cast<const uint8_t*>(map) + 4, 4);
    const size_t bytes = 8 + size_t(payload);
    if (bytes > part->size || a2_load(&decoder, map, bytes) != 0) return false;
    const int chunkSteps = chunkFrames / kSubsampling;
    arena = heap_caps_malloc(a2_arena_bytes(kMaxSteps), MALLOC_CAP_SPIRAM);
    chunkIn = static_cast<float*>(
        heap_caps_malloc(size_t(chunkSteps) * A2_LAYERS_IN * A2_DIN * sizeof(float), MALLOC_CAP_SPIRAM));
    return arena && chunkIn;
}

template <typename T>
void quantizeInput(const float* in, int frames) {
    T* data = featuresIn->get_element_ptr<T>();
    const float invScale = std::ldexp(1.0f, -featuresIn->get_exponent());
    std::fill(data, data + featuresIn->get_size(), 0);
    for (int i = 0; i < frames * kBins; ++i) data[i] = dl::quantize<T>(in[i], invScale);
}

template <typename T>
void appendArgmax(int steps, std::vector<int>& out) {
    const int vocabulary = logits->shape.back();
    const T* data = logits->get_element_ptr<T>();
    for (int t = 0; t < steps; ++t) {
        const T* row = data + t * vocabulary;
        out.push_back(int(std::max_element(row, row + vocabulary) - row));
    }
}

/// Runs one chunk (zero-padded) and encodes its layers into the decoder at `steps`.
void feed(const float* in, int frames, int& steps, std::vector<int>& argmax) {
    if (featuresIn->get_dtype() == dl::DATA_TYPE_INT8) quantizeInput<int8_t>(in, frames);
    else quantizeInput<int16_t>(in, frames);
    for (State& s : states) memcpy(s.input->get_element_ptr(), s.saved.data(), s.saved.size());
    model->run();
    const int n = frames / kSubsampling;
    if (logits->get_dtype() == dl::DATA_TYPE_INT8) appendArgmax<int8_t>(n, argmax);
    else appendArgmax<int16_t>(n, argmax);
    if (steps >= 0 && steps + n <= kMaxSteps) {
        a2_put_layers(chunkIn, 0, n, layer4->get_element_ptr<int16_t>(), layer4->get_exponent(),
                      layer8->get_element_ptr<int16_t>(), layer8->get_exponent());
        a2_encode(&decoder, chunkIn, steps, n, kMaxSteps, arena);
        steps += n;
    } else {
        steps = -1;  // too long: no decoding for this utterance
    }
    for (State& s : states) memcpy(s.saved.data(), s.output->get_element_ptr(), s.saved.size());
}

/// Greedy CTC decoding (blank 0, <unk> 1).
String kanaOf(const std::vector<int>& argmax) {
    std::string text;
    int previous = -1;
    for (int index : argmax) {
        if (index != previous && index > 1 && index < kVocabularySize) text += kVocabulary[index];
        previous = index;
    }
    return text.c_str();
}

void recognize(uint32_t mySession) {
    for (State& s : states) std::fill(s.saved.begin(), s.saved.end(), 0);
    std::vector<int> argmax;
    int computed = 0, fed = 0, steps = 0;
    while (true) {
        if (session != mySession) return;  // a newer utterance started
        const int available = written;
        const bool last = finished && available == written;
        const int frames = std::min(ralomi::frame_count(available), kMaxFrames);
        if (frames > computed) {
            ralomi::log_mel(pcm + computed * ralomi::kHop, available - computed * ralomi::kHop,
                            features + computed * kBins);
            computed = frames;
        }
        while (computed - fed >= chunkFrames || (last && computed > fed)) {
            const int count = std::min(chunkFrames, computed - fed);
            feed(features + fed * kBins, count, steps, argmax);
            fed += count;
        }
        if (last) break;
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(20));
    }

    Result r;
    r.kana = kanaOf(argmax);
    if (steps > 0) {
        a2_result out{};
        a2_generate(&decoder, steps, kMaxSteps, arena, &out);
        std::string json = action_pieces::toJson(out.ids, out.n);
        r.pAct = out.p_act;
        r.accepted = !json.empty() && json != "[]" && out.p_act >= kThreshold;
        r.actions = json.empty() ? "[]" : json.c_str();
    }
    r.tailMs = millis() - endedAt;
    xSemaphoreTake(lock, portMAX_DELAY);
    if (session == mySession) {
        pending = r;
        hasPending = true;
        running = false;
    }
    xSemaphoreGive(lock);
}

void taskMain(void*) {
    uint32_t done = 0;
    while (true) {
        // recognize() also waits on the notification, so a start() can be consumed there: look
        // at the session again before sleeping, and never sleep for long.
        const uint32_t s = session;
        if (running && s != done) {
            done = s;
            recognize(s);
            continue;
        }
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100));
    }
}

void append(const float* samples, int count) {
    int at = written;
    int n = std::min(count, kMaxSamples - at);
    if (n <= 0) return;
    memcpy(pcm + at, samples, n * sizeof(float));
    written = at + n;
}

}  // namespace

bool begin() {
    if (!partitionStartsWith("model", "EDL", 3) || !partitionStartsWith("a2", "A2W2", 4)) {
        log_i("KoeAct: no model in the flash partitions; spoken commands are off");
        return false;
    }
    lock = xSemaphoreCreateMutex();
    ralomi::features_init();
    model = new dl::Model("model", fbs::MODEL_LOCATION_IN_FLASH_PARTITION);
    auto& inputs = model->get_inputs();
    auto& outputs = model->get_outputs();
    if (!inputs.count("log_mel_features") || !outputs.count("logits") || !outputs.count("layer4") ||
        !outputs.count("layer8")) {
        log_e("KoeAct: the model is not KoeAct-JP's stream model");
        return false;
    }
    featuresIn = inputs.at("log_mel_features");
    logits = outputs.at("logits");
    layer4 = outputs.at("layer4");
    layer8 = outputs.at("layer8");
    chunkFrames = featuresIn->shape[1];
    for (auto& [name, tensor] : inputs) {
        if (name.rfind("state_", 0) != 0) continue;
        dl::TensorBase* next = outputs.at("next_" + name);
        states.push_back({tensor, next, std::vector<uint8_t>(tensor->get_size() * tensor->get_dtype_bytes())});
    }
    pcm = static_cast<float*>(heap_caps_malloc(kMaxSamples * sizeof(float), MALLOC_CAP_SPIRAM));
    features = static_cast<float*>(heap_caps_malloc(kMaxFrames * kBins * sizeof(float), MALLOC_CAP_SPIRAM));
    if (!pcm || !features || !loadDecoder()) {
        log_e("KoeAct: out of memory");
        return false;
    }
    // The stack is in PSRAM: the model runs here, and internal RAM is short with Wi-Fi up.
    if (xTaskCreatePinnedToCoreWithCaps(taskMain, "koeact", 32768, nullptr, 3, &task, 0,
                                        MALLOC_CAP_SPIRAM) != pdPASS)
        return false;
    loaded = true;
    log_i("KoeAct: ready (chunk %d frames, %d states), internal free %u, psram free %u", chunkFrames,
          int(states.size()), unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
          unsigned(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)));
    return true;
}

bool ready() { return loaded; }

void start() {
    if (!loaded) return;
    xSemaphoreTake(lock, portMAX_DELAY);
    session = session + 1;
    hasPending = false;
    running = true;
    finished = false;
    written = 0;
    xSemaphoreGive(lock);
    // Leading silence, as the model was trained with.
    static const float zeros[800] = {};
    for (int left = kLeadSamples; left > 0; left -= 800) append(zeros, std::min(left, 800));
    xTaskNotifyGive(task);
}

void push(const int16_t* samples, size_t count) {
    if (!loaded || !running || finished) return;
    float buf[256];
    for (size_t i = 0; i < count; i += 256) {
        int n = std::min<size_t>(256, count - i);
        for (int k = 0; k < n; k++) buf[k] = samples[i + k] / 32768.0f;
        append(buf, n);
    }
    xTaskNotifyGive(task);
}

void finish() {
    if (!loaded || !running || finished) return;
    static const float zeros[800] = {};
    for (int left = kTailSamples; left > 0; left -= 800) append(zeros, std::min(left, 800));
    endedAt = millis();
    finished = true;
    xTaskNotifyGive(task);
}

bool poll(Result& out) {
    if (!loaded) return false;
    bool got = false;
    xSemaphoreTake(lock, portMAX_DELAY);
    if (hasPending) {
        out = pending;
        hasPending = false;
        got = true;
    }
    xSemaphoreGive(lock);
    return got;
}

bool busy() { return running; }

}  // namespace commands
