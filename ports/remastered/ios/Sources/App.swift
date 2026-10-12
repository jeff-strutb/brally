// App.swift: Top Gear Rally Remastered on the iPhone and the iPad. Landscape
// only; the games are drawn into a Metal layer as large as the screen, clear
// of the sensor housing at its sides (the safe area across, the whole height),
// whatever its shape: the Remastered profile's race fills it, its camera
// widened or heightened to the shape, and menus keep the N64's 4:3 inside it.
// The touch surface is over the whole screen. The process
// (ports/remastered/platform/main.c: Top Gear Rally's main loop, Boss Rally
// started at the first race) runs on a thread of its own once the layer is
// laid out.

import Metal
import QuartzCore
import UIKit

@main
final class AppDelegate: UIResponder, UIApplicationDelegate {
    func application(_ application: UIApplication,
                     configurationForConnecting session: UISceneSession,
                     options: UIScene.ConnectionOptions) -> UISceneConfiguration {
        let c = UISceneConfiguration(name: nil, sessionRole: session.role)
        c.delegateClass = SceneDelegate.self
        return c
    }
}

final class SceneDelegate: UIResponder, UIWindowSceneDelegate {
    var window: UIWindow?

    func scene(_ scene: UIScene, willConnectTo session: UISceneSession,
               options connectionOptions: UIScene.ConnectionOptions) {
        guard let ws = scene as? UIWindowScene else { return }
        let w = UIWindow(windowScene: ws)
        w.rootViewController = GameViewController()
        w.makeKeyAndVisible()
        window = w
    }

    // in the background the game is paused (the N64's clock stops) with nothing held
    func sceneWillResignActive(_ scene: UIScene) {
        (window?.rootViewController as? GameViewController)?.surface.reset()
        Events.focus(false)
    }

    func sceneDidBecomeActive(_ scene: UIScene) { Events.focus(true) }
}

final class MetalView: UIView {
    override class var layerClass: AnyClass { CAMetalLayer.self }
}

final class GameViewController: UIViewController {
    let screenView = MetalView()
    let surface = TouchSurface()
    private var started = false

    override func loadView() {
        let root = UIView()
        root.backgroundColor = .black
        screenView.isUserInteractionEnabled = false
        screenView.backgroundColor = .black
        root.addSubview(screenView)
        root.addSubview(surface)
        view = root
        let ml = screenView.layer as! CAMetalLayer
        ml.pixelFormat = .bgra8Unorm
        ml.framebufferOnly = true
        ml.device = MTLCreateSystemDefaultDevice()
        Screen.layer = ml
        AudioFilter.apply()
    }

    override func viewDidLayoutSubviews() {
        super.viewDidLayoutSubviews()
        let b = view.bounds
        surface.frame = b
        // the screen, less the safe area's sides (the iPhone's sensor housing)
        let ins = view.safeAreaInsets
        let w = (b.width - ins.left - ins.right).rounded(), h = b.height
        screenView.frame = CGRect(x: ins.left.rounded(), y: 0, width: w, height: h)
        surface.gameFrame = screenView.frame
        let scale = view.window?.screen.nativeScale ?? UIScreen.main.nativeScale
        let ml = screenView.layer as! CAMetalLayer
        ml.contentsScale = scale
        let px = CGSize(width: (w * scale).rounded(), height: (h * scale).rounded())
        ml.drawableSize = px
        Screen.setPixels(Int32(px.width), Int32(px.height))
    }

    override func viewDidAppear(_ animated: Bool) {
        super.viewDidAppear(animated)
        guard !started, Screen.pixels().0 > 0 else { return }
        started = true
        // the process (ports/remastered/platform/main.c): Top Gear Rally on this
        // thread, Boss Rally on one of its own at the first race; the app's
        // options go to it (a test run's --script; the system's own, before
        // them, do not)
        let all = CommandLine.arguments
        let opts = all.firstIndex { $0.hasPrefix("--") }.map { Array(all[$0...]) } ?? []
        let t = Thread {
            var argv: [UnsafeMutablePointer<CChar>?] = ([all.first ?? "remastered"] + opts).map { strdup($0) } + [nil]
            _ = rm_main(Int32(argv.count - 1), &argv)
        }
        t.name = "remastered"
        t.stackSize = 16 << 20
        t.qualityOfService = .userInteractive
        t.start()
    }

    override var prefersStatusBarHidden: Bool { true }
    override var prefersHomeIndicatorAutoHidden: Bool { true }
    override var preferredScreenEdgesDeferringSystemGestures: UIRectEdge { .all }
    override var supportedInterfaceOrientations: UIInterfaceOrientationMask { .landscape }
}
