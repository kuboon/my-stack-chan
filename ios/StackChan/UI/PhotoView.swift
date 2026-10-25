import StackChanKit
import SwiftUI

/// Takes a photo with the StackChan's camera and shows it, with the share sheet to save it.
struct PhotoView: View {
    var link: DeviceLink
    @Bindable var camera: RemoteCamera
    @Environment(\.dismiss) private var dismiss

    var body: some View {
        NavigationStack {
            VStack(spacing: 16) {
                if let photo = camera.photo {
                    Image(uiImage: photo)
                        .resizable()
                        .scaledToFit()
                        .clipShape(RoundedRectangle(cornerRadius: 12))
                } else {
                    ContentUnavailableView("まだ撮っていません", systemImage: "camera")
                }
                if let timing = camera.timing {
                    Text(timing).font(.footnote).foregroundStyle(.secondary)
                }
                if let error = camera.error {
                    Text(error).foregroundStyle(.red).font(.callout)
                }
                HStack {
                    Picker("大きさ", selection: $camera.size) {
                        Text("640×480").tag(PhotoSize.vga)
                        Text("320×240").tag(PhotoSize.qvga)
                    }
                    .pickerStyle(.segmented)
                    .frame(maxWidth: 240)
                    .disabled(camera.taking)
                    Button {
                        camera.take()
                    } label: {
                        if camera.taking {
                            ProgressView()
                        } else {
                            Label("撮る", systemImage: "camera.fill").font(.title2)
                        }
                    }
                    .buttonStyle(.borderedProminent)
                    .disabled(!link.isConnected || camera.taking)
                    if let photo = camera.photo {
                        let image = Image(uiImage: photo)
                        ShareLink(item: image, preview: SharePreview("スタックちゃんの写真", image: image)) {
                            Label("保存・共有", systemImage: "square.and.arrow.up").font(.title2)
                        }
                        .buttonStyle(.bordered)
                    }
                }
            }
            .padding()
            .frame(maxWidth: .infinity, maxHeight: .infinity)
            .navigationTitle("スタックちゃんのカメラ")
            .toolbar {
                Button("閉じる") { dismiss() }
            }
        }
    }
}
