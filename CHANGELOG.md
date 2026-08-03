# CHANGELOG

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
