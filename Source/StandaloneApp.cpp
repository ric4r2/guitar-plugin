// Standalone application. This is JUCE's stock standalone app with one change: the window always
// opens centred on the screen instead of at the position it was last closed at.
#include <juce_core/system/juce_TargetPlatform.h>
#include <juce_audio_plugin_client/detail/juce_CheckSettingMacros.h>

#include <juce_audio_plugin_client/detail/juce_IncludeSystemHeaders.h>
#include <juce_audio_plugin_client/detail/juce_IncludeModuleHeaders.h>
#include <juce_gui_basics/native/juce_WindowsHooks_windows.h>
#include <juce_audio_plugin_client/detail/juce_PluginUtilities.h>

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>

namespace juce
{
class GuitarDriveStandaloneApp final : public JUCEApplication
{
public:
    GuitarDriveStandaloneApp()
    {
        PropertiesFile::Options options;
        options.applicationName     = CharPointer_UTF8 (JucePlugin_Name);
        options.filenameSuffix      = ".settings";
        options.osxLibrarySubFolder = "Application Support";
        options.folderName          = "";
        appProperties.setStorageParameters (options);
    }

    const String getApplicationName() override              { return CharPointer_UTF8 (JucePlugin_Name); }
    const String getApplicationVersion() override           { return JucePlugin_VersionString; }
    bool moreThanOneInstanceAllowed() override              { return true; }
    void anotherInstanceStarted (const String&) override    {}

    void initialise (const String&) override
    {
        if (Desktop::getInstance().getDisplays().displays.isEmpty())
        {
            quit();
            return;
        }

        // Forget the remembered window position so the window opens centred every time.
        if (auto* settings = appProperties.getUserSettings())
        {
            settings->removeValue ("windowX");
            settings->removeValue ("windowY");
        }

        const Array<StandalonePluginHolder::PluginInOuts> channelConfig;
        auto holder = std::make_unique<StandalonePluginHolder> (appProperties.getUserSettings(), false, String{}, nullptr, channelConfig, false);

        mainWindow = std::make_unique<StandaloneFilterWindow> (getApplicationName(),
                                                               LookAndFeel::getDefaultLookAndFeel().findColour (ResizableWindow::backgroundColourId),
                                                               std::move (holder));
        mainWindow->setVisible (true);
    }

    void shutdown() override
    {
        mainWindow = nullptr;
        appProperties.saveIfNeeded();
    }

    void systemRequestedQuit() override
    {
        if (mainWindow != nullptr)
            mainWindow->pluginHolder->savePluginState();

        if (ModalComponentManager::getInstance()->cancelAllModalComponents())
        {
            Timer::callAfterDelay (100, []()
            {
                if (auto app = JUCEApplicationBase::getInstance())
                    app->systemRequestedQuit();
            });
        }
        else
        {
            quit();
        }
    }

private:
    ApplicationProperties appProperties;
    std::unique_ptr<StandaloneFilterWindow> mainWindow;
};
}

JUCE_CREATE_APPLICATION_DEFINE (juce::GuitarDriveStandaloneApp)
