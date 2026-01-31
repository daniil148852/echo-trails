# Time Rewind Mod

## Overview
This mod adds a time rewind mechanic to Geometry Dash! When you die, instead of immediately respawning, you can rewind time and continue from a few seconds earlier.

## Features
- **Rewind on Death**: Automatically rewind when hitting obstacles
- **Charge System**: Limited rewinds per attempt (configurable)
- **Visual Effects**: VHS-style glitch effect and grayscale overlay during rewind
- **Audio Sync**: Music rewinds along with gameplay
- **Platformer Support**: Works in both Classic and Platformer modes
- **Dual Mode Support**: Both players are rewound in dual sections

## Settings
- **Rewind Charges**: Number of rewinds per attempt (1-10, default: 3)
- **Rewind Duration**: How far back to rewind in seconds (0.5-5.0, default: 2.0)
- **Rewind Speed**: Animation playback speed multiplier (1.0-4.0, default: 2.0)
- **Recording FPS**: State capture frequency (30-240, default: 60)
- **VHS Effect**: Toggle the visual glitch effect
- **Grayscale Effect**: Toggle the grayscale overlay

## Usage
1. Play any level normally
2. When you die, if you have charges remaining, time will automatically rewind
3. Control resumes after the rewind animation completes
4. Charges reset when you restart the level

## Tips
- The rewind captures your position every 1/60th of a second by default
- Higher recording FPS gives smoother rewinds but uses more memory
- You can manually trigger a rewind from the pause menu

## Compatibility
- Geometry Dash 2.2074
- Geode v4.10.2
- Android64 and Windows

## Known Limitations
- Rewind does not work during boss fights or special sequences
- Some visual effects may not rewind perfectly
- Checkpoint respawns will reset rewind buffer
