#include <ayra_pdf/ayra_pdf.h>

#include <cstdio>

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    juce::UnitTestRunner runner;
    runner.setAssertOnFailure (false);
    runner.setPassesAreLogged (true);
    runner.runTestsInCategory ("ayra_pdf", 0x7857);

    int failures = 0;
    int passes = 0;

    for (int index = 0; index < runner.getNumResults(); ++index)
    {
        if (const auto* result = runner.getResult (index))
        {
            failures += result->failures;
            passes += result->passes;
        }
    }

    std::printf ("AYRA_PDF core: passes=%d failures=%d\n",
                 passes,
                 failures);

    return failures == 0 ? 0 : 1;
}
