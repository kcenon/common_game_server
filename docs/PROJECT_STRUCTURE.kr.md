# 프로젝트 구조

> [English](PROJECT_STRUCTURE.md) · **한국어**

```
common_game_server/
├── README.md / README.kr.md      프로젝트 개요 (이중 언어)
├── CLAUDE.md                     에이전트 가독 아키텍처 요약
├── CHANGELOG.md                  릴리스 이력 (Keep a Changelog)
├── CODE_OF_CONDUCT.md            커뮤니티 표준
├── CONTRIBUTING.md               기여 워크플로
├── SECURITY.md                   취약점 보고 정책
├── LICENSE                       BSD 3-Clause 라이선스
├── LICENSE-THIRD-PARTY           의존성 라이선스
├── NOTICES                       SOUP 버전 고지
├── DEPENDENCY_MATRIX.md          상위/하위 버전 표
├── VERSION                       시맨틱 버전 (단일 라인)
├── VERSIONING.md                 SemVer 정책
├── CMakeLists.txt                루트 CMake 구성
├── CMakePresets.json             표준 빌드 프리셋
├── codecov.yml                   커버리지 구성
├── conanfile.py                  Conan 레시피
├── Doxyfile                      Doxygen 구성
├── build.sh                      편의 빌드 스크립트
├── .clang-format                 코드 스타일 (Google 기반, 4-space)
├── .clang-tidy                   정적 분석 규칙
│
├── include/cgs/                  공개 헤더 (설치됨)
│   ├── core/                       Result, 오류 코드, 타입
│   ├── ecs/                        Entity-Component System
│   ├── foundation/                 Foundation 어댑터 인터페이스
│   ├── game/                       게임 로직 타입
│   ├── plugin/                     플러그인 인터페이스
│   ├── service/                    서비스 타입 및 설정
│   ├── cgs.hpp                     umbrella 헤더
│   └── version.hpp                 컴파일 타임 버전 매크로
│
├── src/                          구현 (계층 라이브러리로 컴파일됨)
│   ├── ecs/                        ECS 코어
│   ├── foundation/                 7개의 foundation 어댑터
│   ├── game/                       6개의 게임 로직 시스템
│   ├── plugins/                    플러그인 매니저 + 샘플 플러그인
│   │   ├── manager/
│   │   └── mmorpg/                 샘플 MMORPG 플러그인
│   └── services/                   5개의 마이크로서비스 + 공유 러너
│       ├── shared/                 service_runner 템플릿
│       ├── auth/
│       ├── gateway/
│       ├── game/
│       ├── lobby/
│       └── dbproxy/
│
├── tests/                        테스트 스위트
│   ├── unit/                       단위 테스트 (Google Test)
│   ├── integration/                통합 테스트
│   ├── benchmark/                  성능 벤치마크 (Google Benchmark)
│   ├── load/                       부하 테스트 스크립트
│   └── chaos/                      카오스 / 장애 주입 테스트
│
├── deploy/                       배포 구성
│   ├── docker/                     서비스별 Dockerfile
│   ├── k8s/                        Kubernetes 매니페스트 (HPA, PDB, StatefulSet)
│   ├── monitoring/                 Prometheus + Grafana 대시보드
│   ├── config/                     서비스 설정 템플릿
│   └── docker-compose.yml          로컬 풀스택 개발
│
├── config/                       기본 서비스 설정
├── cmake/                        헬퍼 CMake 모듈
├── scripts/                      유틸리티 스크립트
│
└── docs/                         문서 (kcenon 생태계 템플릿)
    ├── README.md                   색인
    ├── GETTING_STARTED.md          5-10분 튜토리얼
    ├── API_QUICK_REFERENCE.md      1페이지 치트시트
    ├── API_REFERENCE.md / .kr.md   전체 API 문서 (이중 언어)
    ├── ARCHITECTURE.md / .kr.md    시스템 설계 (이중 언어)
    ├── FEATURES.md / .kr.md        기능 매트릭스 (이중 언어)
    ├── BENCHMARKS.md / .kr.md      성능 메트릭 (이중 언어)
    ├── PRODUCTION_QUALITY.md / .kr.md  SLA & 준비도 (이중 언어)
    ├── PROJECT_STRUCTURE.md / .kr.md   본 파일 (이중 언어)
    ├── CHANGELOG.md / .kr.md       릴리스 이력 미러
    ├── ECOSYSTEM.md                kcenon 생태계 위치
    ├── COMPATIBILITY.md            C++/플랫폼 지원
    ├── DEPRECATION.md              사용 중단된 API
    ├── TRACEABILITY.md             교차 문서 참조
    ├── SOUP.md                     서드파티 BOM
    ├── ROADMAP.md                  구현 마일스톤
    │
    ├── adr/                        아키텍처 결정 기록
    ├── advanced/                   심층 분석 참조
    ├── guides/                     How-to & 운영
    ├── contributing/               기여 프로세스
    ├── performance/                성능 분석
    │
    ├── doxygen-awesome-css/        Doxygen 테마 (vendored)
    ├── custom.css                  Doxygen 브랜딩 오버라이드
    ├── header.html                 Doxygen HTML 템플릿
    ├── mainpage.dox / faq.dox / troubleshooting.dox / tutorial_*.dox
    │
    └── archive/                    역사적 보존
        └── sdlc/                   레거시 SDLC 문서 (PRD/SRS/SDS/...)
```

## 핵심 컨벤션

| 컨벤션 | 적용 위치 |
|--------|---------|
| `cgs::` 네임스페이스 접두사 | 모든 공개 API |
| 파일, 클래스, 함수에 `snake_case` | 소스 코드 |
| 상수 및 매크로에 `UPPER_SNAKE_CASE` | 소스 코드 |
| `CGS_*` 매크로 접두사 | 빌드 옵션, 버전 매크로 |
| `*.hpp` 확장자 | 모든 C++ 헤더 |
| `*.cpp` 확장자 | 모든 C++ 구현 파일 |
| `*.dox` 확장자 | Doxygen 콘텐츠 파일 |
| `*.kr.md` 접미사 | 이중 언어 핵심 문서의 한국어 번역 |

## 빌드 출력

`cmake --build` 실행 후 `build/` 아래에 다음이 나타납니다:

```
build/
├── lib/                          정적 라이브러리
├── bin/                          실행 파일 (CGS_BUILD_SERVICES=ON일 때)
└── plugins/                      샘플 플러그인 공유 라이브러리
```

Doxygen 출력 (`doxygen Doxyfile` 후):

```
documents/
└── html/
    └── index.html               생성된 API 문서
```

## 참조

- [`ARCHITECTURE.kr.md`](ARCHITECTURE.kr.md) — 계층이 어떻게 결합되는지
- [`FEATURES.kr.md`](FEATURES.kr.md) — 각 컴포넌트가 제공하는 것
- [`../CONTRIBUTING.md`](../CONTRIBUTING.md) — 변경 사항 기여 방법
