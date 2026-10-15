// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Ohpe standalone application: replaces JUCE's default standalone app
// (JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1) with
// - an own "Audio Settings" page (output device, sample rate, buffer size + latency,
//   MIDI inputs, multi-out routing of the eight mixer channels to eight stereo outputs),
// - all 18 output channels requested from the device (main + 8 stereo aux outputs),
// - translated menus (English / German).

#include <juce_core/system/juce_TargetPlatform.h>

#if JucePlugin_Build_Standalone && JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP

#include <juce_audio_plugin_client/detail/juce_CheckSettingMacros.h>
#include <juce_audio_plugin_client/detail/juce_IncludeSystemHeaders.h>
#include <juce_audio_plugin_client/detail/juce_IncludeModuleHeaders.h>
#include <juce_audio_plugin_client/detail/juce_PluginUtilities.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>

#ifdef Component
 #undef Component
#endif

#include "plugin/Localisation.h"
#include "plugin/PluginProcessor.h"
#include "standalone/AudioSettingsPage.h"
#include "ui/OhpeLookAndFeel.h"
#include "ui/Palette.h"

namespace ohpe
{
using i18n::tr;

class OhpeWindow : public juce::StandaloneFilterWindow, private juce::Timer
{
public:
    OhpeWindow (const juce::String& title, std::unique_ptr<juce::StandalonePluginHolder> holder)
        : StandaloneFilterWindow (title, ui::colour::body, std::move (holder))
    {
        setLookAndFeel (&lnf);
        for (auto* c : getChildren())
            if (auto* b = dynamic_cast<juce::TextButton*> (c); b != nullptr && b != &settingsButton && b->getButtonText() == "Options")
                b->setVisible (false);
        settingsButton.setButtonText (tr ("Audio Settings ..."));
        settingsButton.setTriggeredOnMouseDown (true);
        settingsButton.onClick = [this] { showMenu(); };
        juce::Component::addAndMakeVisible (settingsButton);
        resized();
        startTimer (500);
    }

    ~OhpeWindow() override
    {
        stopTimer();
        settingsWindow = nullptr;
        setLookAndFeel (nullptr);
    }

    void resized() override
    {
        StandaloneFilterWindow::resized();
        settingsButton.setBounds (8, 4, 190, juce::jmax (18, getTitleBarHeight() - 8));
    }

    void showAudioSettings()
    {
        if (settingsWindow != nullptr) { settingsWindow->toFront (true); return; }
        auto* page = new AudioSettingsPage (getDeviceManager(), *static_cast<OhpeProcessor*> (getAudioProcessor()));
        juce::DialogWindow::LaunchOptions o;
        o.content.setOwned (page);
        o.dialogTitle = "Ohpe - " + tr ("Audio Settings");
        o.dialogBackgroundColour = ui::colour::body;
        o.escapeKeyTriggersCloseButton = true;
        o.useNativeTitleBar = true;
        o.resizable = true;
        settingsWindow = o.launchAsync();
    }

private:
    void showMenu()
    {
        juce::PopupMenu m;
        m.addItem (1, tr ("Audio Settings ..."));
        m.addSeparator();
        m.addItem (2, tr ("Save state ..."));
        m.addItem (3, tr ("Load state ..."));
        m.addSeparator();
        m.addItem (4, tr ("Reset to default state"));
        juce::Component::SafePointer<OhpeWindow> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&settingsButton), [safe] (int r) {
            if (safe == nullptr) return;
            switch (r)
            {
                case 1: safe->showAudioSettings(); break;
                case 2: safe->pluginHolder->askUserToSaveState (".ohpe"); break;
                case 3: safe->pluginHolder->askUserToLoadState (".ohpe"); break;
                case 4: safe->resetToDefaultState(); break;
                default: break;
            }
        });
    }

    void timerCallback() override
    {
        // tell the processor how many device outputs exist: aux outputs beyond them play on Main
        if (auto* p = dynamic_cast<OhpeProcessor*> (getAudioProcessor()))
        {
            auto* dev = getDeviceManager().getCurrentAudioDevice();
            p->deviceOutputChannels.store (dev != nullptr ? dev->getActiveOutputChannels().countNumberOfSetBits() : 2);
        }
        if (settingsButton.getButtonText() != tr ("Audio Settings ..."))
        {
            settingsButton.setButtonText (tr ("Audio Settings ..."));
            resized();
        }
    }

    ui::OhpeLookAndFeel lnf;
    juce::TextButton settingsButton;
    juce::Component::SafePointer<juce::DialogWindow> settingsWindow;
};

class OhpeApp : public juce::JUCEApplication
{
public:
    OhpeApp()
    {
        juce::PropertiesFile::Options options;
        options.applicationName = "Ohpe";
        options.filenameSuffix = ".settings";
        options.osxLibrarySubFolder = "Application Support";
       #if JUCE_LINUX || JUCE_BSD
        options.folderName = "~/.config";
       #else
        options.folderName = "";
       #endif
        appProperties.setStorageParameters (options);
    }

    const juce::String getApplicationName() override { return "Ohpe"; }
    const juce::String getApplicationVersion() override { return OHPE_VERSION; }
    bool moreThanOneInstanceAllowed() override { return true; }
    void anotherInstanceStarted (const juce::String&) override {}

    void initialise (const juce::String&) override
    {
        if (juce::Desktop::getInstance().getDisplays().displays.isEmpty()) return;
        juce::AudioDeviceManager::AudioDeviceSetup setup;
        setup.bufferSize = 256;
        setup.useDefaultOutputChannels = false;
        setup.outputChannels.setRange (0, 18, true);
        setup.useDefaultInputChannels = false;
        juce::Array<juce::StandalonePluginHolder::PluginInOuts> channels;
        juce::StandalonePluginHolder::PluginInOuts io;
        io.numIns = 0;
        io.numOuts = 18;
        channels.add (io);
        auto holder = std::make_unique<juce::StandalonePluginHolder> (appProperties.getUserSettings(), false, juce::String(), &setup, channels, true);
        window = std::make_unique<OhpeWindow> ("Ohpe", std::move (holder));
        window->setVisible (true);
    }

    void shutdown() override
    {
        window = nullptr;
        appProperties.saveIfNeeded();
    }

    void systemRequestedQuit() override
    {
        if (window != nullptr) window->pluginHolder->savePluginState();
        if (juce::ModalComponentManager::getInstance()->cancelAllModalComponents())
            juce::Timer::callAfterDelay (100, [] {
                if (auto app = juce::JUCEApplicationBase::getInstance()) app->systemRequestedQuit();
            });
        else
            quit();
    }

private:
    juce::ApplicationProperties appProperties;
    std::unique_ptr<OhpeWindow> window;
};
} // namespace ohpe

juce::JUCEApplicationBase* juce_CreateApplication() { return new ohpe::OhpeApp(); }

#endif
