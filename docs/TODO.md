현재 기능 개발 순서

1. 실제 모델 클릭 및 창 드래그

[x] 화면 좌표를 모델 로컬 공간의 Ray로 변환

[x] AABB 1차 검사 후 모델 삼각형 교차 검사

[x] WM_NCHITTEST의 원형 판정을 모델 피킹 결과로 교체

[x] 창 크기 변경 시 렌더 좌표와 마우스 좌표 비율 보정

[ ] (보류/선택) 투명 텍스처의 alpha clip 영역까지 제외하는 정밀 피킹

[ ] (보류/선택) 서로 다른 FBX/OBJ 모델을 이용한 수동 동작 확인

2. 중력과 모니터 바닥 충돌

[x] WindowPhysics.hpp/.cpp 추가

[x] delta time 기반 수직 속도와 중력 적용

[x] 드래그 중 물리 정지, 드래그 종료 후 낙하

[x] MonitorFromWindow와 rcWork를 이용한 현재 모니터 바닥 계산

[x] 투명 여백이 아닌 모델의 실제 화면 하단을 바닥에 정렬

3. 라이팅 (제외)

[x] 모델 입 주변 아티팩트 확인 후 normal 기반 Blinn-Phong 라이팅 제거

[x] premultiplied alpha 출력 유지 확인

4. 설정 창

[x] 우클릭 더블클릭으로 설정 창 열기

[x] 추후 설정 항목을 추가할 수 있는 기본 설정 창 유지

---

기능1 마우스 인터랙션

[x] 우클릭 드래그로 모델 회전시키기

[x] Quaternion 누적 회전으로 짐벌락 제거

[x] K+L 조작 모드에서 R 키로 모델 회전 초기화

Window.cpp의 메시지 루프(WndProc)에서 WM_RBUTTONDOWN, WM_MOUSEMOVE, WM_RBUTTONUP 이벤트 캐치하기.

마우스가 이동한 거리(Delta X)를 계산해서 Graphics.cpp의 모델 회전 행렬(XMMatrixRotationY)에 꽂아 넣기.

기능2 Win32 우클릭 팝업 메뉴 

[ ] 컨텍스트 메뉴(Context Menu) 띄우기

Win32 API의 CreatePopupMenu()와 TrackPopupMenu()를 사용해 커서 위치에 메뉴창 띄우기.

메뉴에 '애니메이션 1', '애니메이션 2', '종료' 등의 항목 추가하고 클릭 이벤트 연결하기.

대망의 애니메이션 추출
이건 진짜 3D 그래픽스의 꽃이자 헬게이트입니다. 이걸 구현하면 다이렉트X 구조는 완벽하게 마스터했다고 보셔도 됩니다!

[ ] Assimp 애니메이션 데이터 파싱

.fbx 파일에서 정점 데이터뿐만 아니라 '뼈대(Bone)' 정보와 '가중치(Weight)', '키프레임(Keyframe)' 정보 추출하기.

[ ] 버텍스 셰이더(Vertex Shader) 스키닝(Skinning) 구현

정점 구조체(VS_IN)에 뼈 ID와 가중치 정보 추가하기.

HLSL 셰이더에서 C++로부터 뼈대의 현재 프레임 행렬을 넘겨받아 정점들을 실시간으로 구부리고 움직이게 만들기.
