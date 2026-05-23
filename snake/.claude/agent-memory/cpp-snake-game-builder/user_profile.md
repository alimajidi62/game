---
name: user-profile
description: User targets Visual Studio 2022 on Windows, wants classic console games with no external deps
metadata:
  type: user
---

- IDE: Visual Studio 2022 Community (toolset v143, MSBuild 17)
- Platform: Windows 11 Pro
- Preferred game engine: Windows Console API (windows.h + conio.h), no SFML/SDL/OpenGL
- Project structure preference: multi-file (Game.h/cpp, Snake.h/cpp, Renderer.h, main.cpp)
- C++ standard: C++17 (stdcpp17 in vcxproj)
- Working directory: C:\testcode\game\snake
