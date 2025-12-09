# Spec: Countdown Visuals

This spec defines the visual and audio feedback during the match countdown.

## MODIFIED Requirements

### Requirement: The device MUST provide feedback during the match countdown.

The device's presentation layer MUST respond to the `MatchStatus::Countdown` state.

#### Scenario: Match countdown begins
- **Given** the device is in the `READY` state
- **And** the server sets the match status to `Countdown`
- **When** the game timer begins to count down
- **Then** the device's LEDs MUST display a constant, low-brightness white color.

#### Scenario: Match countdown final three seconds
- **Given** the device is displaying the low-brightness countdown effect
- **And** the game timer has 3 seconds or less remaining
- **When** the timer ticks into the final 3 seconds
- **Then** the device's LEDs MUST begin flashing at a high brightness.
- **And** the device MUST emit a short beep once per second.
- **And** the flashing and beeping MUST be synchronized with the game timer.
