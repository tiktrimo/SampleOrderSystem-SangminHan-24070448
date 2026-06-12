# SampleOrderSystem-SangminHan-24070448

## 반도체 시료 생산주문관리 시스템

가상의 반도체 회사 "S-Semi"의 시료 생산·주문 관리를 위한 콘솔 기반 시스템입니다.

## 개발 환경

- **언어**: C++
- **IDE**: Visual Studio
- **빌드**: Visual Studio Solution (.sln)

## 주요 기능

| 메뉴 | 기능 |
|------|------|
| 시료 관리 | 시료 등록 / 조회 / 검색 |
| 시료 주문 | 고객 주문 접수 (RESERVED) |
| 주문 승인/거절 | 재고 확인 후 승인(CONFIRMED/PRODUCING) 또는 거절(REJECTED) |
| 모니터링 | 상태별 주문 수 및 시료별 재고 현황 |
| 생산라인 | 생산 현황 및 대기 큐 조회 (FIFO) |
| 출고 처리 | CONFIRMED 주문 출고 처리 (RELEASE) |

## 주문 상태 흐름

```
RESERVED → (승인) → 재고 충분 → CONFIRMED → RELEASE
                  → 재고 부족 → PRODUCING → CONFIRMED → RELEASE
         → (거절) → REJECTED
```

## 생산 수량 공식

- 실 생산량: `ceil(부족분 / (수율 × 0.9))`
- 총 생산 시간: `평균 생산시간 × 실 생산량`

## 프로젝트 구조

```
SampleOrderSystem-SangminHan-24070448/
  src/
    model/       - 도메인 객체 (Sample.h, Order.h, ProductionLine.h)
    controller/  - 비즈니스 로직 (*.h, *.cpp)
    view/        - 콘솔 UI (*.h, *.cpp)
    repository/  - 파일 기반 데이터 영속성 (*.h, *.cpp)
    service/     - 주문·생산 서비스 레이어 (*.h, *.cpp)
  tests/         - TDD 테스트 코드 (75 tests, 96.6% coverage)
  data/          - 파이프 구분 데이터 파일 (samples.dat, orders.dat)
  docs/          - PRD 등 문서
  SampleOrderSystem.sln
```
