var service = "org.kde.desktopweather";
var objectPath = "/DesktopWeather";
var interfaceName = "org.kde.desktopweather.WindowTracker";

function screenIndexFor(output) {
    var screens = workspace.screens;
    for (var index = 0; index < screens.length; index++) {
        if (screens[index].name === output.name) {
            return index;
        }
    }
    return -1;
}

function snapshotWindow(window) {
    var geometry = window.frameGeometry;
    var output = workspace.screenAt({
        x: geometry.x + geometry.width / 2,
        y: geometry.y + geometry.height / 2
    }) || window.output;
    var screenGeometry = output ? output.geometry : null;
    var screenIndex = output ? screenIndexFor(output) : -1;
    var borderlessFullscreen = Boolean(screenGeometry)
        && !window.fullScreen
        && geometry.x <= screenGeometry.x
        && geometry.y <= screenGeometry.y
        && geometry.x + geometry.width >= screenGeometry.x + screenGeometry.width
        && geometry.y + geometry.height >= screenGeometry.y + screenGeometry.height;
    return {
        title: String(window.caption || ""),
        appId: String(window.resourceClass || window.resourceName || window.desktopFileName || ""),
        screen: screenIndex,
        screenName: output ? String(output.name || "") : "",
        screenWidth: screenGeometry.width,
        screenHeight: screenGeometry.height,
        x: Math.round(geometry.x - (screenGeometry ? screenGeometry.x : 0)),
        y: Math.round(geometry.y - (screenGeometry ? screenGeometry.y : 0)),
        width: Math.round(geometry.width),
        height: Math.round(geometry.height),
        
        fullScreen: Boolean(window.fullScreen),
        borderlessFullscreen: borderlessFullscreen,
        minimized: Boolean(window.minimized),
        active: Boolean(window.active),
        special: Boolean(window.specialWindow)
    };
}

function sendSnapshot() {
    var windows = workspace.stackingOrder
        .filter(function (window) {
            return window
                && !window.deleted
                && !window.minimized
                && !window.hidden;
        })
        .map(snapshotWindow);

    callDBus(service, objectPath, interfaceName, "updateWindows", JSON.stringify(windows));
}

function connectWindowSignal(window, signalName) {
    var signal = window[signalName];
    if (signal && signal.connect) {
        signal.connect(sendSnapshot);
    }
}

function observeWindow(window) {
    connectWindowSignal(window, "frameGeometryChanged");
    connectWindowSignal(window, "captionChanged");
    connectWindowSignal(window, "windowClassChanged");
    connectWindowSignal(window, "fullScreenChanged");
    connectWindowSignal(window, "minimizedChanged");
    connectWindowSignal(window, "activeChanged");
    connectWindowSignal(window, "windowShown");
    connectWindowSignal(window, "windowHidden");
}

workspace.stackingOrder.forEach(observeWindow);
workspace.windowAdded.connect(function (window) {
    observeWindow(window);
    sendSnapshot();
});
workspace.windowRemoved.connect(sendSnapshot);
workspace.windowActivated.connect(sendSnapshot);

sendSnapshot();
var snapshotTimer = new QTimer();
snapshotTimer.interval = 2000;
snapshotTimer.timeout.connect(sendSnapshot);
snapshotTimer.start();
