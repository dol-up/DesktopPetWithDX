# CHANGELOG

## 2026-08-18 00:29 KST

### 구현/수정 내용 요약

- 테스트한 Blinn-Phong 라이팅에서 모델 입 주변 아티팩트가 발생하여 normal 로드와 모든 라이팅 계산을 제거했습니다.
- 셰이더를 텍스처 원본 색상과 premultiplied-alpha 출력만 사용하던 경로로 복구했습니다.
- 모델을 우클릭으로 더블클릭하면 열리는 Win32 설정 창을 추가했습니다.
- 조명 슬라이더는 제거하고 추후 다른 설정을 추가할 수 있는 기본 설정 창만 유지했습니다.
- alpha 기반 정밀 피킹은 필수 작업에서 보류/선택 항목으로 변경했습니다.

### 건드린 파일

- `Asset/Shaders/Shader.hlsl`
- `Source/Graphics/Graphics.cpp`
- `Source/Graphics/Graphics.hpp`
- `Source/Graphics/Shader.cpp`
- `Source/Core/SettingsWindow.cpp`
- `Source/Core/SettingsWindow.hpp`
- `Source/Core/Window.cpp`
- `Source/Core/Window.hpp`
- `Source/Core/main.cpp`
- `Source/Resource/Camera.cpp`
- `Source/Resource/Camera.hpp`
- `Source/Resource/Model.cpp`
- `Source/Resource/Vertex.hpp`
- `docs/TODO.md`

### 검증

- Debug x64 빌드 결과는 오류 0개입니다.
- HLSL `VSMain`과 `PSMain`을 Shader Model 5.0으로 각각 컴파일했습니다.

## 2026-08-03 18:38 KST

### 작업 일시

- 2026-08-03 18:38 (Asia/Seoul)

### 구현/수정 내용 요약

- 파일 선택 창을 통한 FBX/OBJ 모델 선택과 실행 중 모델 교체 기능을 추가했습니다.
- 마지막으로 선택한 모델 경로를 로컬에 저장하고 다음 실행 시 다시 불러오도록 구성했습니다.
- Assimp로 FBX 내장 텍스처와 재질 이름 기반 외부 텍스처를 불러오도록 모델 로딩을 확장했습니다.
- 텍스처가 없는 서브메시는 흰색으로 렌더링하고 이전 SRV가 재사용되지 않도록 처리했습니다.
- 모델 경계 상자를 기준으로 중심과 크기를 정규화하여 서로 다른 크기의 모델을 일관되게 표시하도록 개선했습니다.
- 투명 텍스처의 원본 알파를 보존하고 프리멀티플라이드 알파 색상 블렌딩과 목적지 알파 누적을 적용했습니다.
- 눈썹이 검게 뭉개지는 현상과 뺨·눈 주변에 밝은 테두리 및 패치가 생기는 현상을 수정했습니다.
- 창 이동, 크기 조절, 모델 영역 히트 테스트와 카메라 중심 배치를 정리했습니다.

### 건드린 파일

- `Asset/Shaders/Shader.hlsl`
- `Source/Core/Window.cpp`
- `Source/Core/Window.hpp`
- `Source/Core/main.cpp`
- `Source/Graphics/Graphics.cpp`
- `Source/Graphics/Graphics.hpp`
- `Source/Resource/Camera.cpp`
- `Source/Resource/Camera.hpp`
- `Source/Resource/Model.cpp`
- `Source/Resource/Model.hpp`
- `.gitignore`

### 주의사항

- `last_model.txt`에는 로컬 절대 경로가 기록되므로 Git에서 제외합니다.
- 투명 재질은 픽셀 셰이더에서 RGB에 알파를 미리 곱하고, `ONE / INV_SRC_ALPHA` 색상 블렌딩과 목적지 알파 누적을 사용합니다.

## 2026-08-04 22:43 KST

### 작업 일시

- 2026-08-04 22:43 (Asia/Seoul)

### 구현/수정 내용 요약

- 원형 영역으로 처리하던 창 히트 테스트를 실제 모델 메시 기반 Ray Picking으로 교체했습니다.
- 화면 좌표를 모델 로컬 Ray로 변환하고 AABB 및 삼각형 교차 검사를 수행하도록 `ModelPicker`를 추가했습니다.
- 모델 영역을 좌클릭했을 때만 창을 드래그하고 나머지 투명 영역은 클릭이 통과하도록 연결했습니다.
- 모델의 자동 회전을 제거하고 우클릭 드래그로 직접 회전하도록 변경했습니다.
- 사용자 회전을 정규화된 Quaternion으로 누적하여 Euler 회전의 짐벌락을 제거했습니다.
- `K+L` 조작 모드에서 `R` 키를 한 번 누르면 사용자 회전이 초기화되도록 추가했습니다.
- 우클릭 드래그 회전 방향을 반전하고 Pitch 감도를 Yaw보다 낮게 조정했습니다.
- 향후 중력과 Blinn-Phong 라이팅 작업 순서를 `docs/TODO.md`에 정리했습니다.

### 건드린 파일

- `DesktopPetWithDX.vcxproj`
- `DesktopPetWithDX.vcxproj.filters`
- `Source/Core/Window.cpp`
- `Source/Core/Window.hpp`
- `Source/Core/main.cpp`
- `Source/Graphics/Graphics.cpp`
- `Source/Graphics/Graphics.hpp`
- `Source/Interaction/ModelPicker.cpp`
- `Source/Interaction/ModelPicker.hpp`
- `Source/Resource/Model.cpp`
- `Source/Resource/Model.hpp`
- `docs/TODO.md`

### 주의사항

- 현재 피킹은 모델의 실제 삼각형을 판정하지만 텍스처의 투명한 alpha clip 영역까지 제외하지는 않습니다.
- `R` 키 회전 초기화는 `K+L`로 전환하는 조작 모드에서만 동작하며 모델의 기본 축 보정 회전은 유지합니다.
- 우클릭 회전 감도는 Yaw `0.01`, Pitch `0.0075` rad/pixel입니다.
- Debug x64 빌드 결과는 오류 0개이며 기존 `NOMINMAX` 재정의 경고가 남아 있습니다.

## 2026-08-12 22:58 KST

### 작업 일시

- 2026-08-12 22:58 (Asia/Seoul)

### 구현/수정 내용 요약

- 창의 수직 속도와 프레임 `deltaTime`을 이용해 모델이 아래로 낙하하는 기본 중력을 구현했습니다.
- 긴 프레임으로 인한 급격한 이동을 막도록 `deltaTime`에 상한을 적용하고, 서브픽셀 이동량을 누적해 저속 낙하도 부드럽게 처리했습니다.
- 모델을 좌클릭 드래그하거나 우클릭 회전하는 동안 중력을 정지하고, 조작을 끝내면 새 낙하를 시작하도록 연결했습니다.
- `K+L` 조작 모드에서는 W/A/S/D 창 이동 및 크기 조절과 충돌하지 않도록 물리를 일시 정지했습니다.
- `MonitorFromWindow`와 `MONITORINFO::rcWork`를 사용해 현재 모니터의 작업 표시줄 위를 착지 바닥으로 계산했습니다.
- 현재 모델 회전과 창 크기를 반영한 정점 투영 결과로 화면상 모델 최하단을 계산하여 투명한 창 여백이 아닌 보이는 모델을 기준으로 착지하도록 구현했습니다.
- 중력 및 모니터 바닥 충돌 항목을 `docs/TODO.md`에서 완료 처리했습니다.

### 건드린 파일

- `DesktopPetWithDX.vcxproj`
- `DesktopPetWithDX.vcxproj.filters`
- `Source/Core/Window.hpp`
- `Source/Core/main.cpp`
- `Source/Graphics/Graphics.cpp`
- `Source/Graphics/Graphics.hpp`
- `Source/Physics/WindowPhysics.cpp`
- `Source/Physics/WindowPhysics.hpp`
- `docs/TODO.md`

### 주의사항

- 현재 물리는 Y축 낙하와 바닥 위치 보정만 담당하며 무게중심, 각속도, 관성, 충돌 토크는 계산하지 않습니다.
- 모델 최하단은 현재 정적인 원본 메시 정점으로 계산하므로 스켈레탈 애니메이션 구현 후에는 캡슐 Collider 또는 발 기준 Collider로 교체할 예정입니다.
- 낙하 중 자세 복원과 `Falling → Landing → Idle` 전환은 애니메이션 상태 시스템을 구현할 때 통합합니다.
- Debug x64 빌드 결과는 오류 0개이며 기존 `NOMINMAX` 재정의 경고 2개가 남아 있습니다.
