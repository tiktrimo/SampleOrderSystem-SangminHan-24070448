# SampleOrderSystem-SangminHan-24070448

> **[POC Stage]** 현재 개념 검증(Proof of Concept) 단계입니다. POC 검증 완료 후 본 구현을 진행합니다.

## 반도체 시료 생산주문관리 시스템

가상의 반도체 회사 "S-Semi"의 시료 생산·주문 관리를 위한 콘솔 기반 시스템입니다.

## 시스템 개요

주문 급증으로 인한 엑셀/메모장 기반 관리의 한계를 극복하기 위해 개발하는 체계적인 시료 생산주문관리 시스템입니다.

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
src/
  model/       - 도메인 객체 (Sample, Order, ProductionLine)
  controller/  - 비즈니스 로직
  view/        - 콘솔 UI
  repository/  - 데이터 영속성 (JSON)
  service/     - 주문·생산 서비스 레이어
data/          - JSON 데이터 저장소
docs/          - 문서 (PRD 등)
```
