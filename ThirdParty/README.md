# Third-party sources

Dear ImGui v1.92.5 is vendored in `imgui/` from
https://github.com/ocornut/imgui/tree/v1.92.5 under the MIT license.
The core files and Win32/D3D11 backends are compiled via `ImGui.props`.
The upstream license is retained in `imgui/LICENSE.txt`.
Korean UI glyphs use Windows' installed Malgun Gothic font at runtime; no font is redistributed.
