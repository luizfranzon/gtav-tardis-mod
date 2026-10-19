// TARDIS control rules (domain), ported rule-for-rule from TARDIS Rewrite (gmod_tardis/modules sh_flight,
// sh_float, sh_handbrake, sh_spin, sh_cloak, sh_physlock). Pure: no game calls.
#pragma once

struct TardisState
{
	bool flight = false;
	bool floating = false;   // GMod "float" (anti-gravs)
	bool floatfirst = false; // float was asked for by the pilot, so flight turning off keeps it
	bool handbrake = false;
	bool spin = true;
	int spinDir = -1; // -1 anti-clockwise, 1 clockwise (GMod "spin-dir")
	bool spinLastCycle = false;
	bool cloak = false;
	bool physlock = false;
	bool shield = false; // not in GMod (its shields are a health bar): reflects attacks
	int speedOffset = 0; // km/h added to flight's top speed (not in GMod)

	int SpinDir() const { return spin ? spinDir : 0; }

	bool SetFloat(bool on)
	{
		if (!on && (flight || floatfirst)) return false; // CanTurnOffFloat
		floating = on;
		return true;
	}

	bool ToggleFloat()
	{
		bool on = !floating;
		if (flight) floatfirst = !floatfirst;
		else floatfirst = on;
		return SetFloat(on);
	}

	bool SetFlight(bool on)
	{
		if (on && handbrake) return false; // CanTurnOnFlight (no power system in this port)
		flight = on;
		SetFloat(on);
		return true;
	}

	bool ToggleFlight() { return SetFlight(!flight); }

	// flight_interrupt_to_float defaults to true in GMod
	void InterruptFlight()
	{
		if (!flight) return;
		floatfirst = true;
		ToggleFlight();
	}

	bool ToggleHandbrake()
	{
		handbrake = !handbrake;
		if (handbrake) InterruptFlight();
		return true;
	}

	void CycleSpinDir()
	{
		bool last = spinLastCycle;
		spinLastCycle = !last;
		if (last) spin = false;
		else { spin = true; spinDir = -spinDir; }
	}
};
