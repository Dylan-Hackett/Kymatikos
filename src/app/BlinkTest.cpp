#include "daisy_patch_sm.h"

using namespace daisy;
using namespace daisy::patch_sm;

static DaisyPatchSM patch;

// Ensure the vector table points at the QSPI image so interrupts/logging work.
static void ConfigureVectorTableForQSPI()
{
#if defined(STM32H750xx) || defined(STM32H7XX) || defined(__STM32H750xx_H)
    SCB->VTOR = 0x90040000UL;
#endif
}

int main(void)
{
    ConfigureVectorTableForQSPI();
    patch.Init();

    // Start USB CDC logging without waiting for a host connection.
    patch.StartLog(false);
    patch.PrintLine("QSPI blink test booted.");

    uint32_t last_led_toggle = System::GetNow();
    uint32_t last_print      = last_led_toggle;
    bool     led_on          = false;
    uint32_t heartbeat_count = 0;

    while(true)
    {
        uint32_t now = System::GetNow();

        // Toggle LED every 250 ms so a failure is obvious.
        if(now - last_led_toggle >= 250)
        {
            last_led_toggle = now;
            led_on          = !led_on;
            patch.SetLed(led_on);
        }

        // Print once per second to confirm USB logging.
        if(now - last_print >= 1000)
        {
            last_print = now;
            patch.PrintLine("Heartbeat %lu", static_cast<unsigned long>(heartbeat_count++));
        }

        System::Delay(1);
    }
}
