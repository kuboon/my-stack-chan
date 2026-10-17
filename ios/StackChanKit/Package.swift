// swift-tools-version: 6.2
// The parts of the app that do not need a device: the pairing crypto and the wire protocol
// (web/server/docs/protocol.md). `swift test` runs on macOS, and on Linux with swift-crypto.
import PackageDescription

let package = Package(
    name: "StackChanKit",
    platforms: [.iOS(.v26), .macOS(.v26)],
    products: [.library(name: "StackChanKit", targets: ["StackChanKit"])],
    dependencies: [
        .package(url: "https://github.com/apple/swift-crypto.git", "3.0.0"..<"5.0.0"),
    ],
    targets: [
        .target(
            name: "StackChanKit",
            dependencies: [
                .product(name: "Crypto", package: "swift-crypto", condition: .when(platforms: [.linux])),
            ]
        ),
        .testTarget(
            name: "StackChanKitTests",
            dependencies: ["StackChanKit"],
            resources: [.copy("vectors.json")]
        ),
    ]
)
