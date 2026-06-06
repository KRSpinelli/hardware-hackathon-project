# Threshold Justifications

## Temperature Alert — `HOT_THRESH_dC`

**Value:** 28.0°C ✅ confirmed  
**Firmware constant:** `HOT_THRESH_dC 280` (stored as tenths of °C)

Per **CIBSE Guide A (Table 1.5)**, office temperatures should not exceed 
**28°C for more than 1% of occupied hours** — making 28°C the recognised 
upper tolerance limit for occupied office spaces. **ANSI/ASHRAE Standard 
55-2023** defines the thermal comfort zone upper boundary at 26°C for 
sedentary summer office work (~1.2 MET), meaning anything above 26°C is 
already outside the comfort zone. At 28°C, conditions are at the absolute 
ceiling of tolerability. Alerting at 28°C is therefore evidence-based and 
appropriate as a "take action now" trigger.

**Sources:**
- ANSI/ASHRAE Standard 55-2023: *Thermal Environmental Conditions for Human Occupancy*
- CIBSE Guide A: *Environmental Design*, Table 1.5

---

## Sitting Duration Alert — `TOO_LONG_MS`

**Value:** 30 minutes ✅ confirmed  
**Firmware constant:** `TOO_LONG_MS 1800000UL` (milliseconds)

**NHS UK Chief Medical Officers' Physical Activity Guidelines** recommend 
breaking up long periods of sitting with at least light activity. The 
**Health and Safety Executive (HSE)** advises breaking sitting every 
**30–60 minutes** with 1–2 minutes of movement. **Mayo Clinic** research 
shows sitting for as little as **30 minutes** without standing begins to 
contribute to negative health outcomes. A **2019 Cochrane systematic review** 
(PMC6646952) found breaks at **20–40 minute intervals** reduce 
musculoskeletal symptoms in office workers. 30 minutes is evidence-based 
and appropriate.

**Sources:**
- NHS UK: *Why sitting too much is bad for us* (nhs.uk/live-well/exercise)
- Health and Safety Executive (HSE): *Sedentary behaviour in the workplace*
- Mayo Clinic: *Sedentary behavior research*, 2021
- Cochrane Review PMC6646952: *Work-break schedules for preventing 
  musculoskeletal symptoms in office workers*, 2019

---

## Summary

| Firmware Constant | Value | Status | Primary Source |
|---|---|---|---|
| `HOT_THRESH_dC` | 28.0°C | ✅ Evidence-based | CIBSE Guide A, ASHRAE 55-2023 |
| `TOO_LONG_MS` | 30 minutes | ✅ Evidence-based | NHS, HSE, Cochrane 2019 |
