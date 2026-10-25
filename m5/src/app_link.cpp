#include "app_link.h"

#include <ArduinoJson.h>
#include <algorithm>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <esp_random.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <mbedtls/base64.h>

#include "audio.h"
#include "config.h"
#include "net.h"
#include "pairing_crypto.h"
#include "settings.h"

namespace app_link {
namespace {

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
QueueHandle_t commands;
SemaphoreHandle_t lock;  // guards everything below

uint8_t key[32];
bool hasKey = false;

PairingState pairState = PairingState::None;
uint8_t pairPriv[32];
uint8_t pairDevicePub[32];
uint8_t pairAppPub[32];
uint32_t pairCode = 0;
uint32_t pairStarted = 0;

struct Pending {
    uint32_t id;
    uint8_t nonce[pairing::kNonceSize];
    uint32_t since;
};
Pending pending[4];
uint32_t authedId = 0;  // 0: none
String textBuf;         // a text message arriving in pieces

int rng(void*, unsigned char* out, size_t len) {
    esp_fill_random(out, len);
    return 0;
}

struct Guard {
    Guard() { xSemaphoreTake(lock, portMAX_DELAY); }
    ~Guard() { xSemaphoreGive(lock); }
};

void push(CommandType type, const char* text = "", float x = 0, float y = 0, const char* secret = "") {
    Command c{};
    c.type = type;
    strlcpy(c.text, text ? text : "", sizeof c.text);
    strlcpy(c.secret, secret, sizeof c.secret);
    c.x = x;
    c.y = y;
    xQueueSend(commands, &c, 0);
    memset(&c, 0, sizeof c);
}

/// `wifi.connect`: opens the password and hands it to the loop, or answers bad_request.
void handleWifiConnect(AsyncWebSocketClient* client, const JsonDocument& doc) {
    std::string ssid = doc["ssid"] | "";
    uint8_t nonce[pairing::kWifiNonceSize];
    uint8_t sealed[64 + pairing::kWifiTagSize];
    size_t sealedLen = 0;
    std::string password;
    bool ok = !ssid.empty() && ssid.size() <= 32 &&
              pairing::base64Decode(doc["nonce"] | "", nonce, sizeof nonce) &&
              pairing::base64Decode(doc["password"] | "", sealed, sizeof sealed, sealedLen);
    if (ok) {
        uint8_t wifiKey[32];
        {
            Guard g;
            pairing::wifiKey(key, wifiKey);
        }
        ok = pairing::openWifiPassword(wifiKey, ssid, nonce, sealed, sealedLen, password) &&
             password.size() <= 63;
        memset(wifiKey, 0, sizeof wifiKey);
    }
    if (!ok) {
        client->text("{\"type\":\"wifi.result\",\"ok\":false,\"error\":\"bad_request\"}");
        return;
    }
    push(CommandType::WifiConnect, ssid.c_str(), 0, 0, password.c_str());
    std::fill(password.begin(), password.end(), '\0');
}

void sendJson(AsyncWebServerRequest* req, int code, const JsonDocument& doc) {
    String body;
    serializeJson(doc, body);
    req->send(code, "application/json", body);
}

// --- HTTP ---------------------------------------------------------------------

void handleInfo(AsyncWebServerRequest* req) {
    JsonDocument doc;
    doc["name"] = net::name();
    doc["wifi"] = net::mode() == net::Mode::STA ? "sta" : "ap";
    doc["model"] = STACKCHAN_MODEL;
    doc["firmware"] = STACKCHAN_FIRMWARE_VERSION;
    {
        Guard g;
        doc["paired"] = hasKey;
    }
    sendJson(req, 200, doc);
}

void handlePairStart(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
    if (index != 0 || len != total) {
        req->send(413);
        return;
    }
    JsonDocument in;
    if (deserializeJson(in, data, len)) {
        req->send(400);
        return;
    }
    uint8_t appPub[32];
    if (!pairing::base64Decode(in["public_key"] | "", appPub, 32)) {
        req->send(400);
        return;
    }
    JsonDocument out;
    {
        Guard g;
        if (hasKey) {
            req->send(409);
            return;
        }
        esp_fill_random(pairPriv, sizeof pairPriv);
        if (!pairing::publicKey(pairPriv, pairDevicePub, rng, nullptr)) {
            req->send(500);
            return;
        }
        memcpy(pairAppPub, appPub, 32);
        pairCode = pairing::pairingCode(pairDevicePub, pairAppPub);
        pairStarted = millis();
        pairState = PairingState::Waiting;
        out["public_key"] = pairing::base64Encode(pairDevicePub, 32);
    }
    sendJson(req, 200, out);
}

void handlePairStatus(AsyncWebServerRequest* req) {
    JsonDocument doc;
    {
        Guard g;
        switch (pairState) {
            case PairingState::Waiting: doc["state"] = "waiting"; break;
            case PairingState::Approved: doc["state"] = "approved"; break;
            case PairingState::Rejected: doc["state"] = "rejected"; break;
            default: doc["state"] = "none"; break;
        }
    }
    sendJson(req, 200, doc);
}

// --- WebSocket ----------------------------------------------------------------

void handleText(AsyncWebSocketClient* client, const String& text) {
    JsonDocument doc;
    if (deserializeJson(doc, text)) return;
    const char* type = doc["type"] | "";

    bool authed;
    {
        Guard g;
        authed = authedId == client->id();
    }
    if (!authed) {
        if (strcmp(type, "auth") != 0) return;
        uint8_t mac[32], expect[32];
        bool ok = false;
        uint32_t replaced = 0;
        {
            Guard g;
            for (auto& p : pending) {
                if (p.id != client->id()) continue;
                if (hasKey && pairing::base64Decode(doc["mac"] | "", mac, 32)) {
                    pairing::wsMac(key, p.nonce, expect);
                    ok = pairing::equalConstantTime(mac, expect, 32);
                }
                p.id = 0;
            }
            if (ok) {
                replaced = authedId;
                authedId = client->id();
            }
        }
        if (!ok) {
            client->close(4001, "auth failed");
            return;
        }
        if (replaced) ws.close(replaced, 4002, "replaced");
        JsonDocument ready;
        ready["type"] = "ready";
        ready["name"] = net::name();
        String s;
        serializeJson(ready, s);
        client->text(s);
        push(CommandType::ClientReady);
        return;
    }

    if (!strcmp(type, "listen.start")) push(CommandType::ListenStart);
    else if (!strcmp(type, "listen.end")) push(CommandType::ListenEnd);
    else if (!strcmp(type, "speak.start")) {
        audio::beginSpeech();  // here, so that PCM right behind it is kept
        push(CommandType::SpeakStart, doc["text"] | "");
    } else if (!strcmp(type, "speak.end")) {
        audio::endSpeech();
        push(CommandType::SpeakEnd);
    } else if (!strcmp(type, "face")) push(CommandType::Face, doc["expression"] | "neutral");
    else if (!strcmp(type, "look")) push(CommandType::Look, "", doc["yaw"] | 0.0f, doc["pitch"] | 0.0f);
    else if (!strcmp(type, "state")) push(CommandType::State, doc["state"] | "idle");
    else if (!strcmp(type, "photo.take")) push(CommandType::PhotoTake, doc["size"] | "vga");
    else if (!strcmp(type, "wifi.scan")) push(CommandType::WifiScan);
    else if (!strcmp(type, "wifi.connect")) handleWifiConnect(client, doc);
    else if (!strcmp(type, "wifi.forget")) push(CommandType::WifiForget);
}

void onEvent(AsyncWebSocket*, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data,
             size_t len) {
    switch (type) {
        case WS_EVT_CONNECT: {
            Pending p{client->id(), {}, millis()};
            esp_fill_random(p.nonce, sizeof p.nonce);
            {
                Guard g;
                // A free slot, or else the oldest socket still waiting to authenticate.
                Pending* slot = nullptr;
                for (auto& q : pending)
                    if (q.id == 0) {
                        slot = &q;
                        break;
                    }
                if (!slot) {
                    slot = &pending[0];
                    for (auto& q : pending)
                        if (q.since < slot->since) slot = &q;
                }
                *slot = p;
            }
            JsonDocument hello;
            hello["type"] = "hello";
            hello["nonce"] = pairing::base64Encode(p.nonce, sizeof p.nonce);
            String s;
            serializeJson(hello, s);
            client->text(s);
            break;
        }
        case WS_EVT_DISCONNECT: {
            bool wasAuthed = false;
            {
                Guard g;
                for (auto& q : pending)
                    if (q.id == client->id()) q.id = 0;
                if (authedId == client->id()) {
                    authedId = 0;
                    wasAuthed = true;
                }
            }
            if (wasAuthed) push(CommandType::ClientGone);
            break;
        }
        case WS_EVT_DATA: {
            auto* info = static_cast<AwsFrameInfo*>(arg);
            bool authed;
            {
                Guard g;
                authed = authedId == client->id();
            }
            if (info->opcode == WS_BINARY || (info->opcode == WS_CONTINUATION && info->message_opcode == WS_BINARY)) {
                if (authed) audio::pushSpeech(data, len);
                break;
            }
            // Text: gather the pieces of one message.
            if (info->index == 0) textBuf = "";
            if (textBuf.length() + len > 4096) break;
            textBuf.concat(reinterpret_cast<const char*>(data), len);
            if (info->final && info->index + len == info->len) handleText(client, textBuf);
            break;
        }
        default:
            break;
    }
}

}  // namespace

void begin() {
    commands = xQueueCreate(16, sizeof(Command));
    lock = xSemaphoreCreateMutex();
    hasKey = settings::authKey(key);

    ws.onEvent(onEvent);
    server.addHandler(&ws);
    server.on("/info", HTTP_GET, handleInfo);
    server.on("/pair/status", HTTP_GET, handlePairStatus);
    server.on(
        "/pair/start", HTTP_POST, [](AsyncWebServerRequest*) {}, nullptr, handlePairStart);
    server.onNotFound([](AsyncWebServerRequest* req) { req->send(404); });
    server.begin();
}

bool poll(Command& out) { return xQueueReceive(commands, &out, 0) == pdTRUE; }

bool connected() {
    Guard g;
    return authedId != 0;
}

void sendText(const String& json) {
    uint32_t id;
    {
        Guard g;
        id = authedId;
    }
    if (id) ws.text(id, json);
}

void sendAudio(const int16_t* samples, size_t count) {
    uint32_t id;
    {
        Guard g;
        id = authedId;
    }
    if (id) ws.binary(id, reinterpret_cast<const uint8_t*>(samples), count * sizeof(int16_t));
}

bool sendPhoto(const camera::Photo& photo) {
    uint32_t id;
    {
        Guard g;
        id = authedId;
    }
    if (!id) return false;
    // Written straight into one buffer: the Base64 of a VGA photo is around 100 KB.
    char frames[96] = "";
    for (int i = 0, n = 0; i < photo.frames; i++)
        n += snprintf(frames + n, sizeof frames - n, "%s%u", i ? "," : "", unsigned(photo.frameMs[i]));
    char head[256];
    int headLen = snprintf(head, sizeof head,
                           "{\"type\":\"photo\",\"width\":%d,\"height\":%d,\"ms\":{\"init\":%u,\"warmup\":%u,"
                           "\"shot\":%u,\"jpeg\":%u,\"frames\":[%s]},\"missed\":%d,\"jpeg\":\"",
                           photo.width, photo.height, unsigned(photo.initMs), unsigned(photo.warmupMs),
                           unsigned(photo.shotMs), unsigned(photo.jpegMs), frames, photo.missed);
    static const char tail[] = "\"}";
    size_t b64Len = 4 * ((photo.len + 2) / 3);
    size_t total = headLen + b64Len + sizeof tail;  // tail's NUL is room for mbedtls' own
    char* buf = static_cast<char*>(ps_malloc(total));
    if (!buf) return false;
    memcpy(buf, head, headLen);
    size_t written = 0;
    bool ok = mbedtls_base64_encode(reinterpret_cast<unsigned char*>(buf + headLen), b64Len + 1, &written,
                                    photo.jpeg, photo.len) == 0;
    if (ok) {
        memcpy(buf + headLen + written, tail, sizeof tail - 1);
        ok = ws.text(id, buf, headLen + written + sizeof tail - 1);
    }
    free(buf);
    return ok;
}

PairingState pairingState() {
    Guard g;
    return pairState;
}

String pairingCode() {
    Guard g;
    return pairing::formatCode(pairCode).c_str();
}

void approvePairing() {
    Guard g;
    if (pairState != PairingState::Waiting) return;
    uint8_t shared[32];
    if (!pairing::sharedSecret(pairPriv, pairAppPub, shared, rng, nullptr)) {
        pairState = PairingState::Rejected;
    } else {
        pairing::authKey(shared, pairAppPub, pairDevicePub, key);
        settings::setAuthKey(key);
        hasKey = true;
        pairState = PairingState::Approved;
    }
    memset(shared, 0, sizeof shared);
    memset(pairPriv, 0, sizeof pairPriv);
}

void rejectPairing() {
    Guard g;
    if (pairState != PairingState::Waiting) return;
    pairState = PairingState::Rejected;
    memset(pairPriv, 0, sizeof pairPriv);
}

bool paired() {
    Guard g;
    return hasKey;
}

void unpair() {
    uint32_t id;
    {
        Guard g;
        settings::clearAuthKey();
        hasKey = false;
        memset(key, 0, sizeof key);
        pairState = PairingState::None;
        id = authedId;
    }
    if (id) ws.close(id, 4003, "unpaired");
}

void update() {
    uint32_t now = millis();
    uint32_t expired[4] = {};
    {
        Guard g;
        if (pairState == PairingState::Waiting && now - pairStarted > config::kPairingTimeoutMs) {
            pairState = PairingState::Rejected;
            memset(pairPriv, 0, sizeof pairPriv);
        }
        for (int i = 0; i < 4; i++) {
            if (pending[i].id && now - pending[i].since > config::kAuthTimeoutMs) {
                expired[i] = pending[i].id;
                pending[i].id = 0;
            }
        }
    }
    for (uint32_t id : expired)
        if (id) ws.close(id, 4001, "auth timeout");
    ws.cleanupClients(4);
}

}  // namespace app_link
