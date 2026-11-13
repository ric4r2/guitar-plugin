// GuitarDriveSnapshot <output folder>: saves wah.png, pre.png, amp.png, eq.png and post.png of the editor window.
#include "PluginEditor.h"
#include "PluginProcessor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File folder (argc >= 2 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    folder.createDirectory();

    GuitarDriveProcessor proc;
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    auto* editor = dynamic_cast<GuitarDriveEditor*> (ed.get());
    if (editor == nullptr)
        return 1;

    const char* names[] = { "wah", "pre", "amp", "eq", "post" };
    for (int v = 0; v < 5; ++v)
    {
        editor->showView (v);
        const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
        auto file = folder.getChildFile (juce::String (names[v]) + ".png");
        file.deleteFile();
        juce::FileOutputStream out (file);
        juce::PNGImageFormat().writeImageToStream (image, out);
        std::printf ("wrote %s (%dx%d)\n", file.getFullPathName().toRawUTF8(), image.getWidth(), image.getHeight());
    }
    return 0;
}
