#include <XBase/WebBridge.h>

#include <XBase/Capabilities.h>
#include <XBase/Host.h>
#include <XBase/Log.h>
#include <XBase/Player.h>
#include <XBase/Teleport.h>
#include <XBase/Vehicle.h>
#include <XBase/WebView.h>
#include <XBase/Weapon.h>
#include <XBase/World.h>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>

namespace {

constexpr int ProtocolVersion = 1;

std::unordered_map<std::string, XBase::WebBridge::MethodHandler> s_customMethods;

// 页面脚本注入，网页侧用 window.xbase.call 拿 Promise，用 on 订阅原生事件
const char* const ClientScript = R"JS(
(function () {
    if (!window.chrome || !window.chrome.webview) {
        return;
    }

    var pending = new Map();
    var listeners = new Map();
    var nextId = 1;

    window.chrome.webview.addEventListener('message', function (event) {
        var data = event.data;
        if (!data) {
            return;
        }
        if (typeof data.id === 'number' && pending.has(data.id)) {
            var entry = pending.get(data.id);
            pending.delete(data.id);
            if (data.ok) {
                entry.resolve(data.result);
            } else {
                entry.reject(new Error(data.error || 'xbase call failed'));
            }
            return;
        }
        if (typeof data.event === 'string' && listeners.has(data.event)) {
            listeners.get(data.event).forEach(function (fn) {
                fn(data.payload);
            });
        }
    });

    window.xbase = {
        call: function (method, params) {
            return new Promise(function (resolve, reject) {
                var id = nextId++;
                pending.set(id, { resolve: resolve, reject: reject });
                window.chrome.webview.postMessage({ id: id, method: method, params: params || {} });
            });
        },
        on: function (event, fn) {
            if (!listeners.has(event)) {
                listeners.set(event, []);
            }
            listeners.get(event).push(fn);
        },
        capabilities: function () {
            return window.xbase.call('bridge.capabilities');
        }
    };
})();
)JS";

const char* CapabilityName(XBase::CapabilitySupport support) {
    switch (support) {
    case XBase::CapabilitySupport::Supported: return "supported";
    case XBase::CapabilitySupport::Partial: return "partial";
    case XBase::CapabilitySupport::Unsupported:
    default: return "unsupported";
    }
}

void Reply(const XBase::Json::Value& id, const XBase::Json::Value& result) {
    XBase::Json::Value response;
    response.Set("id", id);
    response.Set("ok", true);
    response.Set("result", result);
    XBase::WebView::PostJson(response.Serialize(false));
}

void Fail(const XBase::Json::Value& id, const std::string& error) {
    XBase::Json::Value response;
    response.Set("id", id);
    response.Set("ok", false);
    response.Set("error", error);
    XBase::WebView::PostJson(response.Serialize(false));
}

bool RequireCapability(
    const XBase::Json::Value& id,
    XBase::FeatureCapability capability,
    const char* method) {
    if (XBase::GetCapabilitySupport(capability) != XBase::CapabilitySupport::Unsupported) {
        return true;
    }
    Fail(id, std::string("unsupported on this game: ") + method);
    return false;
}

XBase::Json::Value CapabilityReport() {
    struct Entry {
        const char* method;
        XBase::FeatureCapability capability;
    };

    static const Entry entries[] = {
        {"player.snapshot", XBase::FeatureCapability::PlayerBasicState},
        {"player.heal", XBase::FeatureCapability::PlayerBasicState},
        {"player.armour", XBase::FeatureCapability::PlayerBasicState},
        {"player.money", XBase::FeatureCapability::PlayerBasicState},
        {"player.wanted", XBase::FeatureCapability::PlayerBasicState},
        {"player.kill", XBase::FeatureCapability::PlayerBasicState},
        {"player.moveRelative", XBase::FeatureCapability::PlayerMovement},
        {"vehicle.snapshot", XBase::FeatureCapability::VehicleBasic},
        {"vehicle.spawn", XBase::FeatureCapability::VehicleSpawn},
        {"vehicle.repair", XBase::FeatureCapability::VehicleBasic},
        {"vehicle.unflip", XBase::FeatureCapability::VehicleBasic},
        {"vehicle.colors", XBase::FeatureCapability::VehicleColors},
        {"vehicle.doors", XBase::FeatureCapability::VehicleDoors},
        {"world.getTime", XBase::FeatureCapability::WorldTime},
        {"world.setTime", XBase::FeatureCapability::WorldTime},
        {"world.weather", XBase::FeatureCapability::WorldWeather},
        {"world.gameSpeed", XBase::FeatureCapability::WorldGameSpeed},
        {"world.gravity", XBase::FeatureCapability::WorldGravity},
        {"world.freezeTime", XBase::FeatureCapability::WorldFreezeTime},
        {"teleport.to", XBase::FeatureCapability::TeleportBasic},
        {"teleport.forward", XBase::FeatureCapability::TeleportBasic},
        {"teleport.marker", XBase::FeatureCapability::TeleportBasic},
        {"weapon.give", XBase::FeatureCapability::WeaponGive},
        {"weapon.giveAll", XBase::FeatureCapability::WeaponGive},
        {"weapon.infiniteAmmo", XBase::FeatureCapability::WeaponRuntimeEffects},
    };

    XBase::Json::Value methods;
    for (const Entry& entry : entries) {
        methods.Set(entry.method, XBase::Json::Value(CapabilityName(XBase::GetCapabilitySupport(entry.capability))));
    }

    XBase::Json::Value result;
    result.Set("protocol", XBase::Json::Value(ProtocolVersion));
    result.Set("methods", methods);
    return result;
}

XBase::Json::Value PlayerSnapshotJson() {
    const XBase::Player::PlayerSnapshot snapshot = XBase::Player::GetSnapshot();

    XBase::Json::Value position;
    position.Set("x", XBase::Json::Value(static_cast<double>(snapshot.position.x)));
    position.Set("y", XBase::Json::Value(static_cast<double>(snapshot.position.y)));
    position.Set("z", XBase::Json::Value(static_cast<double>(snapshot.position.z)));

    XBase::Json::Value result;
    result.Set("valid", XBase::Json::Value(snapshot.valid));
    result.Set("position", position);
    result.Set("health", XBase::Json::Value(static_cast<double>(snapshot.health)));
    result.Set("armour", XBase::Json::Value(static_cast<double>(snapshot.armour)));
    result.Set("money", XBase::Json::Value(snapshot.money));
    result.Set("wantedLevel", XBase::Json::Value(snapshot.wantedLevel));
    return result;
}

XBase::Json::Value VehicleSnapshotJson() {
    const XBase::Vehicle::VehicleSnapshot snapshot = XBase::Vehicle::GetSnapshot();

    XBase::Json::Value colors;
    colors.Set("primary", XBase::Json::Value(snapshot.colors.primary));
    colors.Set("secondary", XBase::Json::Value(snapshot.colors.secondary));

    XBase::Json::Value result;
    result.Set("valid", XBase::Json::Value(snapshot.modelId != 0));
    result.Set("modelId", XBase::Json::Value(static_cast<int>(snapshot.modelId)));
    result.Set("health", XBase::Json::Value(static_cast<double>(snapshot.health)));
    result.Set("colors", colors);
    result.Set("lights", XBase::Json::Value(snapshot.lights));
    result.Set("locked", XBase::Json::Value(snapshot.locked));
    return result;
}

void HandleMessage(const std::string& message) {
    const XBase::Json::Value request = XBase::Json::Value::Parse(message);
    if (!request.IsObject()) {
        return;
    }

    const XBase::Json::Value id = request["id"];
    const std::string method = request["method"].AsString();
    const XBase::Json::Value params = request["params"];
    if (method.empty()) {
        Fail(id, "missing method");
        return;
    }

    if (method == "bridge.capabilities") {
        Reply(id, CapabilityReport());
        return;
    }

    // 宿主注册的方法优先，便于覆盖或扩展默认表
    const auto custom = s_customMethods.find(method);
    if (custom != s_customMethods.end()) {
        if (!custom->second) {
            Fail(id, std::string("handler missing: ") + method);
            return;
        }
        Reply(id, custom->second(params));
        return;
    }

    if (method == "player.snapshot") {
        Reply(id, PlayerSnapshotJson());
        return;
    }
    if (method == "player.heal") {
        if (!RequireCapability(id, XBase::FeatureCapability::PlayerBasicState, "player.heal")) return;
        XBase::Player::Heal();
        Reply(id, XBase::Json::Value());
        return;
    }
    if (method == "player.armour") {
        if (!RequireCapability(id, XBase::FeatureCapability::PlayerBasicState, "player.armour")) return;
        XBase::Player::GiveArmour();
        Reply(id, XBase::Json::Value());
        return;
    }
    if (method == "player.money") {
        if (!RequireCapability(id, XBase::FeatureCapability::PlayerBasicState, "player.money")) return;
        XBase::Player::SetMoney(params["amount"].AsInt());
        Reply(id, XBase::Json::Value());
        return;
    }
    if (method == "player.wanted") {
        if (!RequireCapability(id, XBase::FeatureCapability::PlayerBasicState, "player.wanted")) return;
        XBase::Player::SetWantedLevel(params["level"].AsInt());
        Reply(id, XBase::Json::Value());
        return;
    }
    if (method == "player.kill") {
        if (!RequireCapability(id, XBase::FeatureCapability::PlayerBasicState, "player.kill")) return;
        XBase::Player::Kill();
        Reply(id, XBase::Json::Value());
        return;
    }
    if (method == "player.moveRelative") {
        if (!RequireCapability(id, XBase::FeatureCapability::PlayerMovement, "player.moveRelative")) return;
        const bool moved = XBase::Player::MoveRelative(
            static_cast<float>(params["forward"].AsNumber()),
            static_cast<float>(params["right"].AsNumber()),
            static_cast<float>(params["up"].AsNumber()));
        Reply(id, XBase::Json::Value(moved));
        return;
    }

    if (method == "vehicle.snapshot") {
        Reply(id, VehicleSnapshotJson());
        return;
    }
    if (method == "vehicle.spawn") {
        if (!RequireCapability(id, XBase::FeatureCapability::VehicleSpawn, "vehicle.spawn")) return;
        XBase::Vehicle::SpawnOptions options;
        options.asDriver = params["asDriver"].AsBool(true);
        options.aircraftInAir = params["aircraftInAir"].AsBool(true);
        options.cleanupPrevious = params["cleanupPrevious"].AsBool(true);
        const XBase::Vehicle::SpawnResult result =
            XBase::Vehicle::SpawnEx(static_cast<unsigned int>(params["model"].AsInt()), options);
        Reply(id, XBase::Json::Value(result.success));
        return;
    }
    if (method == "vehicle.repair") {
        if (!RequireCapability(id, XBase::FeatureCapability::VehicleBasic, "vehicle.repair")) return;
        XBase::Vehicle::Repair();
        Reply(id, XBase::Json::Value());
        return;
    }
    if (method == "vehicle.unflip") {
        if (!RequireCapability(id, XBase::FeatureCapability::VehicleBasic, "vehicle.unflip")) return;
        XBase::Vehicle::Unflip();
        Reply(id, XBase::Json::Value());
        return;
    }
    if (method == "vehicle.colors") {
        if (!RequireCapability(id, XBase::FeatureCapability::VehicleColors, "vehicle.colors")) return;
        XBase::Vehicle::Colors colors = XBase::Vehicle::GetColors();
        if (!params["primary"].IsNull()) colors.primary = params["primary"].AsInt();
        if (!params["secondary"].IsNull()) colors.secondary = params["secondary"].AsInt();
        XBase::Vehicle::SetColors(colors);
        Reply(id, XBase::Json::Value());
        return;
    }
    if (method == "vehicle.doors") {
        if (!RequireCapability(id, XBase::FeatureCapability::VehicleDoors, "vehicle.doors")) return;
        XBase::Vehicle::OpenDoor(params["index"].AsInt());
        Reply(id, XBase::Json::Value());
        return;
    }

    if (method == "world.getTime") {
        int hour = 0;
        int minute = 0;
        XBase::World::GetTime(hour, minute);
        XBase::Json::Value result;
        result.Set("hour", XBase::Json::Value(hour));
        result.Set("minute", XBase::Json::Value(minute));
        Reply(id, result);
        return;
    }
    if (method == "world.setTime") {
        if (!RequireCapability(id, XBase::FeatureCapability::WorldTime, "world.setTime")) return;
        XBase::World::SetTime(params["hour"].AsInt(), params["minute"].AsInt());
        Reply(id, XBase::Json::Value());
        return;
    }
    if (method == "world.weather") {
        if (!RequireCapability(id, XBase::FeatureCapability::WorldWeather, "world.weather")) return;
        XBase::World::SetWeather(params["id"].AsInt(), params["lock"].AsBool(false));
        Reply(id, XBase::Json::Value());
        return;
    }
    if (method == "world.gameSpeed") {
        if (!RequireCapability(id, XBase::FeatureCapability::WorldGameSpeed, "world.gameSpeed")) return;
        XBase::World::SetGameSpeed(static_cast<float>(params["value"].AsNumber(1.0)));
        Reply(id, XBase::Json::Value());
        return;
    }
    if (method == "world.gravity") {
        if (!RequireCapability(id, XBase::FeatureCapability::WorldGravity, "world.gravity")) return;
        XBase::World::SetGravity(static_cast<float>(params["value"].AsNumber()));
        Reply(id, XBase::Json::Value());
        return;
    }
    if (method == "world.freezeTime") {
        if (!RequireCapability(id, XBase::FeatureCapability::WorldFreezeTime, "world.freezeTime")) return;
        XBase::World::SetFreezeTime(params["enable"].AsBool(true));
        Reply(id, XBase::Json::Value());
        return;
    }

    if (method == "teleport.to") {
        if (!RequireCapability(id, XBase::FeatureCapability::TeleportBasic, "teleport.to")) return;
        const bool moved = XBase::Teleport::To(
            static_cast<float>(params["x"].AsNumber()),
            static_cast<float>(params["y"].AsNumber()),
            static_cast<float>(params["z"].AsNumber()),
            params["interior"].AsInt());
        Reply(id, XBase::Json::Value(moved));
        return;
    }
    if (method == "teleport.forward") {
        if (!RequireCapability(id, XBase::FeatureCapability::TeleportBasic, "teleport.forward")) return;
        const bool moved = XBase::Teleport::Forward(static_cast<float>(params["distance"].AsNumber(10.0)));
        Reply(id, XBase::Json::Value(moved));
        return;
    }
    if (method == "teleport.marker") {
        if (!RequireCapability(id, XBase::FeatureCapability::TeleportBasic, "teleport.marker")) return;
        const bool moved = XBase::Teleport::Marker(params["underwater"].AsBool(false));
        Reply(id, XBase::Json::Value(moved));
        return;
    }

    if (method == "weapon.give") {
        if (!RequireCapability(id, XBase::FeatureCapability::WeaponGive, "weapon.give")) return;
        const bool given = XBase::Weapon::Give(
            static_cast<unsigned int>(params["type"].AsInt()),
            static_cast<unsigned int>(params["ammo"].AsInt(999)));
        Reply(id, XBase::Json::Value(given));
        return;
    }
    if (method == "weapon.giveAll") {
        if (!RequireCapability(id, XBase::FeatureCapability::WeaponGive, "weapon.giveAll")) return;
        Reply(id, XBase::Json::Value(XBase::Weapon::GiveAll()));
        return;
    }
    if (method == "weapon.infiniteAmmo") {
        if (!RequireCapability(id, XBase::FeatureCapability::WeaponRuntimeEffects, "weapon.infiniteAmmo")) return;
        Reply(id, XBase::Json::Value(XBase::Weapon::SetInfiniteAmmo(params["enable"].AsBool(true))));
        return;
    }

    if (method == "ui.notice") {
        const std::string text = params["text"].AsString();
        if (text.empty()) {
            Fail(id, "missing text");
            return;
        }
        XBase::Host::QueueMessage(text.c_str());
        Reply(id, XBase::Json::Value());
        return;
    }

    Fail(id, std::string("unknown method: ") + method);
}

bool s_installed = false;

} // namespace

namespace XBase::WebBridge {

bool RegisterMethod(const std::string& method, MethodHandler handler) {
    if (method.empty() || !handler) {
        return false;
    }
    s_customMethods[method] = std::move(handler);
    return true;
}

void UnregisterMethod(const std::string& method) {
    s_customMethods.erase(method);
}

void Install() {
    if (s_installed) {
        return;
    }

    XBase::WebView::SetMessageHandler(HandleMessage);
    XBase::WebView::InjectScript(ClientScript);
    s_installed = true;
    XBase::Log::Info("WebBridge: 已注册网页调用通道");
}

void Shutdown() {
    if (!s_installed) {
        return;
    }

    XBase::WebView::SetMessageHandler(nullptr);
    s_installed = false;
}

bool IsInstalled() {
    return s_installed;
}

bool Emit(const std::string& event, const Json::Value& payload) {
    if (!s_installed || event.empty()) {
        return false;
    }

    Json::Value message;
    message.Set("event", Json::Value(event));
    message.Set("payload", payload);
    return XBase::WebView::PostJson(message.Serialize(false));
}

} // namespace XBase::WebBridge
