//
// Copyright (C) 2017~2017 by CSSlayer
// wengxt@gmail.com
//
// This library is free software; you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as
// published by the Free Software Foundation; either version 2.1 of the
// License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library; see the file COPYING. If not,
// see <http://www.gnu.org/licenses/>.
//
#ifndef _CHTTRANS_CHTTRANS_H_
#define _CHTTRANS_CHTTRANS_H_

#include "notifications_public.h"
#include <fcitx-config/configuration.h>
#include <fcitx-config/enum.h>
#include <fcitx-config/iniparser.h>
#include <fcitx-utils/i18n.h>
#include <fcitx/action.h>
#include <fcitx/addoninstance.h>
#include <fcitx/addonmanager.h>
#include <fcitx/instance.h>
#include <unordered_set>

FCITX_CONFIG_ENUM(ChttransEngine, Native, OpenCC);

FCITX_CONFIGURATION(
    ChttransConfig,
    fcitx::Option<ChttransEngine> engine{this, "Engine", _("Translate engine"),
                                         ChttransEngine::OpenCC};
    fcitx::Option<fcitx::KeyList> hotkey{
        this, "Hotkey", _("Toggle key"), {fcitx::Key("Control+Shift+F")}};
    fcitx::HiddenOption<std::vector<std::string>> enabledIM{
        this, "EnabledIM", _("Enabled Input Methods")};);

enum class ChttransIMType { Simp, Trad, Other };

class ChttransBackend {
public:
    virtual ~ChttransBackend() {}
    bool load() {
        if (!loaded_) {
            loadResult_ = loadOnce();
            loaded_ = true;
        }
        return loadResult_;
    }
    virtual std::string convertSimpToTrad(const std::string &) = 0;
    virtual std::string convertTradToSimp(const std::string &) = 0;

protected:
    virtual bool loadOnce() = 0;

private:
    bool loaded_ = false;
    bool loadResult_ = false;
};

class Chttrans final : public fcitx::AddonInstance {
    class ToggleAction : public fcitx::Action {
    public:
        ToggleAction(Chttrans *parent) : parent_(parent) {}

        std::string shortText(fcitx::InputContext *ic) const override {
            return parent_->convertType(ic) == ChttransIMType::Trad
                       ? _("Traditional Chinese")
                       : _("Simplified Chinese");
        }
        std::string icon(fcitx::InputContext *ic) const override {
            return parent_->convertType(ic) == ChttransIMType::Trad
                       ? "fcitx-chttrans-active"
                       : "fcitx-chttrans-inactive";
        }

        void activate(fcitx::InputContext *ic) override {
            return parent_->toggle(ic);
        }

    private:
        Chttrans *parent_;
    };

public:
    Chttrans(fcitx::Instance *instance);

    void reloadConfig() override;
    void save() override;
    const fcitx::Configuration *getConfig() const override { return &config_; }
    void setConfig(const fcitx::RawConfig &config) override {
        config_.load(config, true);
        fcitx::safeSaveAsIni(config_, "conf/chttrans.conf");
        reloadConfig();
    }

    bool needConvert(fcitx::InputContext *inputContext);
    ChttransIMType convertType(fcitx::InputContext *inputContext);
    std::string convert(ChttransIMType type, const std::string &str);
    void toggle(fcitx::InputContext *inputContext);

    fcitx::AddonInstance *notifications() {
        if (!notifications_) {
            notifications_ =
                instance_->addonManager().addon("notifications", true);
        }
        return notifications_;
    }

private:
    fcitx::Instance *instance_;
    ChttransConfig config_;
    fcitx::AddonInstance *notifications_ = nullptr;
    std::unique_ptr<fcitx::HandlerTableEntry<fcitx::EventHandler>>
        eventHandler_;
    std::unordered_map<ChttransEngine, std::unique_ptr<ChttransBackend>,
                       fcitx::EnumHash>
        backends_;
    std::unordered_set<std::string> enabledIM_;
    fcitx::ScopedConnection outputFilterConn_;
    fcitx::ScopedConnection commitFilterConn_;
    ToggleAction toggleAction_{this};
};

#endif // _CHTTRANS_CHTTRANS_H_
