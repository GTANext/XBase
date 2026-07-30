#include <XBase/XBase.h>
#include "plugin.h"

static void ProcessAll() {
    XBase::Core::Process();
}

static void OnDraw() {
    XBase::BulletAssist::Draw();
}

class XBasePlugin {
public:
    XBasePlugin() {
        XBase::Log::Init();
        XBase::Config::Init();
        XBase::I18n::Init();
        XBase::Core::Init(XBase::Core::AllDomains);
        XBase::BulletAssist::Init();

        plugin::Events::gameProcessEvent += ProcessAll;
        plugin::Events::drawingEvent += OnDraw;
        plugin::Events::initGameEvent += [] {
            XBase::Core::NotifyGameInit();
            XBase::Config::Init();
        };
    }

    ~XBasePlugin() {
        XBase::Core::Shutdown();
        XBase::Config::Save();
        XBase::Log::Shutdown();
    }
} xbasePlugin;
