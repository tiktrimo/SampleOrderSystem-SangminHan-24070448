# Detailed Level Design (DLD)
## 반도체 시료 생산주문관리 시스템 — S-Semi

**작성자**: Sangmin Han (24070448)  
**최종 수정**: 2026-06-12  
**버전**: 1.0

---

## 1. 시스템 개요

콘솔 기반 반도체 시료 생산주문관리 시스템. 시료 등록·조회, 주문 접수/승인/거절, 실시간 생산 시뮬레이션, 재고 모니터링, 출고 처리 기능을 제공한다.

### 1.1 기술 스택

| 항목 | 내용 |
|------|------|
| 언어 | C++17 |
| 빌드 | MSBuild Debug\|x64, `/utf-8` |
| OS | Windows 10+ (ANSI escape, Console API) |
| 데이터 저장 | 파이프 구분 텍스트 파일 (`data/`) |
| 테스트 | 자체 TDD 프레임워크 (`TestRunner.h`) |

---

## 2. 아키텍처

### 2.1 레이어 구조

```
┌─────────────────────────────────────────────┐
│                  main.cpp                   │  진입점 · 메뉴 루프 · UI 조합
├──────────────┬──────────────────────────────┤
│  View Layer  │       ConsoleView             │  콘솔 출력 전담
├──────────────┼──────────────────────────────┤
│  Service     │  OrderService                 │  주문 상태 전환 규칙
│  Layer       │  ProductionService            │  생산 큐·공식 계산
│              │  MonitorService               │  스냅샷 집계
├──────────────┼──────────────────────────────┤
│  Controller  │  OrderController              │  주문 인메모리 CRUD
│  Layer       │  SampleController             │  시료 인메모리 CRUD
├──────────────┼──────────────────────────────┤
│  Repository  │  OrderRepository              │  orders.dat ↔ Order
│  Layer       │  SampleRepository             │  samples.dat ↔ Sample
├──────────────┼──────────────────────────────┤
│  Model       │  Order · Sample               │  도메인 데이터 구조체
└──────────────┴──────────────────────────────┘
```

### 2.2 데이터 흐름

```
사용자 입력
    │
    ▼
main.cpp (메뉴 함수)
    │  oc.placeOrder / oc.findById / sc.findById …
    ▼
Controller (인메모리 상태 관리)
    │  OrderService::approve / reject / release
    │  ProductionService::enqueue / complete
    ▼
Service (비즈니스 규칙)
    │
    ▼
Repository (파일 I/O)
    │  save / update / findAll …
    ▼
data/orders.dat
data/samples.dat
```

---

## 3. 도메인 모델

### 3.1 Order

```cpp
struct Order {
    std::string  orderId;       // ORD-YYYYMMDD-NNNN
    std::string  sampleId;      // 시료 ID 참조
    std::string  customerName;
    int          quantity  = 0;
    OrderStatus  status    = OrderStatus::RESERVED;
    int          shortage  = 0; // PRODUCING 전환 시 부족분 (파일 저장)
};
```

#### OrderStatus 상태 전이

```
RESERVED ──[approve, 재고 충분]──► CONFIRMED ──[release]──► RELEASE
         ──[approve, 재고 부족]──► PRODUCING ──[생산완료]──► CONFIRMED
         ──[reject]─────────────► REJECTED
```

### 3.2 Sample

```cpp
struct Sample {
    std::string id;
    std::string name;
    double      avgProductionTimeMin;  // 단위당 생산시간(min) — 시뮬: 1min=1sec
    double      yield;                 // 수율 (0.0 ~ 1.0)
    int         stock = 0;             // 현재 재고 (DB 기준 실제값)
};
```

### 3.3 ProductionItem (인메모리 전용)

```cpp
struct ProductionItem {
    std::string  orderId;
    std::string  sampleId;
    std::string  sampleName;
    int          orderQuantity;
    int          shortage;          // 승인 시 부족분
    int          actualProduction;  // 실 생산량
    double       totalTimeMin;      // 총 생산 시간(sec 시뮬)
    std::time_t  startTime;         // enqueue 시각 (Unix timestamp)
};
```

---

## 4. 파일 스키마 (영속성)

### 4.1 data/samples.dat

```
{id}|{name}|{avgProductionTimeMin}|{yield}|{stock}
```

예시:
```
AD001|HiDe|0.500000|0.500000|100
AS01|Jit|0.300000|0.900000|50
```

### 4.2 data/orders.dat

```
{orderId}|{sampleId}|{customerName}|{quantity}|{status}|{shortage}
```

- `shortage` 필드: 이전 버전 파일 호환을 위해 없을 경우 0으로 파싱
- `status`: RESERVED / REJECTED / PRODUCING / CONFIRMED / RELEASE

예시:
```
ORD-20260612-0001|AD001|삼성전자|150|PRODUCING|50
ORD-20260612-0002|AS01|SK하이닉스|30|CONFIRMED|0
```

---

## 5. 모듈 상세 설계

### 5.1 OrderController

| 메서드 | 시그니처 | 설명 |
|--------|----------|------|
| `placeOrder` | `string(sampleId, customerName, qty)` | RESERVED 주문 생성, ID 반환 |
| `getAll` | `vector<Order>()` | 전체 주문 목록 |
| `getByStatus` | `vector<Order>(OrderStatus)` | 상태별 필터 |
| `findById` | `optional<Order>(orderId)` | 단건 조회 |
| `updateOrder` | `bool(Order)` | 인메모리 업데이트 |
| `getOrderCount` | `int()` | REJECTED·RELEASE 제외 건수 |
| `syncSequence` | `void()` | 파일 로드 후 `orderSeq_` 동기화 |

**주문번호 생성 규칙**: `ORD-{YYYYMMDD}-{NNNN}` (4자리 zero-padding 시퀀스)

### 5.2 SampleController

| 메서드 | 시그니처 | 설명 |
|--------|----------|------|
| `addSample` | `void(Sample)` | 중복 ID 무시 |
| `findById` | `optional<Sample>(id)` | 단건 조회 |
| `searchByName` | `vector<Sample>(keyword)` | 이름 부분 검색 |
| `updateStock` | `void(id, delta)` | delta(±)로 재고 가감 |
| `getTotalStock` | `int()` | 전체 재고 합계 |

### 5.3 OrderService (static)

| 메서드 | 전제 조건 | 동작 |
|--------|-----------|------|
| `approve(Order&, Sample&)` | status == RESERVED | stock≥qty → CONFIRMED / stock<qty → PRODUCING |
| `reject(Order&)` | status == RESERVED | → REJECTED |
| `release(Order&)` | status == CONFIRMED | → RELEASE |

> **재고 차감 시점**: `approve`에서는 재고를 변경하지 않음.  
> 재고 차감은 `release` 이후 `menuRelease`에서 `-order.quantity` 처리.

### 5.4 ProductionService

#### 핵심 공식

```
실 생산량  = ceil(부족분 / (수율 × 0.9))
총 생산시간 = 평균생산시간(min) × 실 생산량   ← 시뮬레이션: 1min = 1sec
```

| 메서드 | 설명 |
|--------|------|
| `calcActualProduction(shortage, yield)` | 실 생산량 공식 계산 |
| `calcTotalTime(avgMin, actual)` | 총 생산 시간 계산 |
| `enqueue(Order, Sample, shortage)` | ProductionItem 생성 후 FIFO 큐 push. `startTime = time(nullptr)` |
| `peek()` | 큐 front 조회 (팝 없음) |
| `complete(Order&, Sample&)` | 큐 front 팝, order→CONFIRMED, sample.stock += actualProduction |
| `getQueueItems()` | 큐 전체 복사본 반환 (표시용) |

**스케줄링 전략**: FIFO (`std::queue<ProductionItem>`)

### 5.5 MonitorService (static)

```
buildSnapshot(orders, samples):
  1. REJECTED 제외, 상태별 주문 건수 집계
  2. 각 시료별 pendingQuantity = RESERVED + PRODUCING 주문량 합산
  3. classifyStock(stock, pending):
       stock == 0        → DEPLETED
       stock < pending   → LOW
       otherwise         → SUFFICIENT
```

### 5.6 Repository 공통 패턴

```
생성자 → loadAll() → cache_ 초기화
save/update/remove → cache_ 변경 → saveAll() (전체 재기록)
findById/findAll → cache_ 읽기 (파일 미접근)
```

---

## 6. 생산 진행률 계산

생산은 실제 타이머 기반으로 진행되며 DB는 **완료 시에만** 업데이트된다.

```
진행률(prog) = min(1.0, elapsed / totalTimeMin)
  elapsed   = now - item.startTime  (초 단위)
  totalTimeMin = 시뮬레이션 초 단위 (1min → 1sec)

현재생산량(display) = (int)(prog × actualProduction)
```

화면 표시(모니터링·메인메뉴)에서는 `curProd`를 실제 재고에 더해 가상 재고를 보여준다.  
이는 **표시 전용**이며 DB에는 기록되지 않는다.

---

## 7. 재고 생명주기

```
주문 접수(RESERVED)
  └─ 재고 변동 없음

주문 승인(approve)
  ├─ 재고 충분 → CONFIRMED: 재고 변동 없음
  └─ 재고 부족 → PRODUCING: 재고 변동 없음, shortage 저장

생산 완료(complete)
  └─ sample.stock += actualProduction
     (DB: sc.updateStock(+actProd), sr.update(s))

출고(release)
  └─ sample.stock -= order.quantity
     (DB: sc.updateStock(-qty), sr.update(s))
```

**재고 변동 요약**:

| 이벤트 | 재고 변동 | DB 기록 |
|--------|-----------|---------|
| 주문 접수 | 없음 | — |
| 승인 (충분/부족) | 없음 | — |
| 생산 완료 | +actualProduction | ✓ |
| 출고 | -order.quantity | ✓ |

---

## 8. 시스템 시작 복원 절차

```
1. SampleRepository.findAll()  → SampleController에 적재
2. OrderRepository.findAll()   → OrderController.orders_에 적재
3. OrderController.syncSequence()  → orderSeq_ 동기화 (중복 ID 방지)
4. PRODUCING 상태 주문 탐색:
     shortage = o.shortage > 0 ? o.shortage : o.quantity
     ps.enqueue(o, *s, shortage)
     → startTime = time(nullptr)  ← 재시작 시 생산 타이머 초기화
```

> **Known Limitation**: 재시작 시 `startTime`이 초기화되므로 생산 진행률이 0%로 리셋된다. `startTime`을 Order 파일에 저장하면 해결 가능 (미구현).

---

## 9. 메인 루프 흐름

```
main()
  ├─ 초기화 (Repository 로드, syncSequence, PRODUCING 큐 복원)
  └─ while (choice != 0):
       autoCompleteProduction()   ← 완료된 생산 즉시 처리
       displayStock = getTotalStock() + curProd(진행분)
       showMainMenu(...)
       cin >> choice
       switch(choice):
         1 → menuSampleManage
         2 → menuPlaceOrder
         3 → menuApproveReject
         4 → autoComplete + buildSnapshot + curProd 반영 + showMonitor
         5 → menuProductionLine  (200ms 폴링 실시간 렌더)
         6 → menuRelease
```

### autoCompleteProduction 로직

```
while ps.hasNext():
    item = ps.peek()
    elapsed = now - item.startTime
    if elapsed < item.totalTimeMin: break   ← 아직 진행 중
    ps.complete(o, s)
    sc.updateStock(sampleId, +actProd)
    or_.update(o); sr.update(s)
```

---

## 10. 콘솔 UI

### 10.1 색상 체계 (ANSI)

| 용도 | 코드 | 색상 |
|------|------|------|
| 강조 헤더 | `\033[1;36m` | Bold Cyan |
| 성공/충분 | `\033[32m` | Green |
| 경고/부족 | `\033[33m` | Yellow |
| 오류/거절 | `\033[31m` | Red |
| 출고 | `\033[34m` | Blue |
| 생산중 | `\033[36m` | Cyan |
| 보조 텍스트 | `\033[2;37m` | Dim White |

### 10.2 화면 지우기

`clsConsole()`: Windows Console API 사용 (`FillConsoleOutputCharacter` + `SetConsoleCursorPosition`)  
ANSI `\033[2J\033[H`는 일부 터미널에서 미동작하여 대체.

### 10.3 생산라인 실시간 렌더링

- 200ms 간격 `_kbhit()` 폴링 + `Sleep(200)` × 5 = 1초 주기
- `setCursorVisible(false)` 로 커서 깜빡임 억제
- 진행률 바: `█`(filled) / `░`(empty) × 30칸

---

## 11. 테스트 구조

```
tests/
  TestRunner.h          자체 매크로 (TEST, ASSERT_*)
  test_sample.cpp       시료 모델·컨트롤러 (8개)
  test_order.cpp        주문 컨트롤러 (9개)
  test_order_service.cpp OrderService 상태 전이 (10개)
  test_production.cpp   ProductionService 공식·큐 (9개)
  test_monitor.cpp      MonitorService 집계 (7개)
  test_repository.cpp   파일 영속성 CRUD (18개)
  test_scenario.cpp     통합 시나리오 (14개)
```

**총 75개 테스트, 0 실패**

### 주요 시나리오 테스트

| 시나리오 | 검증 내용 |
|----------|-----------|
| `scenario_sufficient_stock_full_flow` | RESERVED→CONFIRMED→RELEASE 정상 흐름 |
| `scenario_insufficient_stock_production_full_flow` | PRODUCING→CONFIRMED 생산 흐름 |
| `scenario_fifo_production_queue_three_orders` | FIFO 순서 보장 |
| `scenario_sequential_approval_no_stock_deduction` | 승인 시 재고 미차감 확인 |
| `scenario_production_output_covers_order_quantity` | 공식 정확성: ceil(95/(0.8×0.9))=132 |

---

## 12. 알려진 제약 및 개선 가능 항목

| 항목 | 현재 동작 | 개선 방향 |
|------|-----------|-----------|
| 생산 재시작 | 시작 타이머 초기화 (0%부터 재시작) | Order에 `startTime` 저장 후 복원 |
| 동시 승인 재고 | 승인 시 차감 없어 복수 주문 과초과 가능 | 가예약(reserved stock) 필드 추가 |
| Repository 쓰기 | 변경마다 파일 전체 재기록 | 추가/갱신 시 delta 방식으로 최적화 |
| 생산 진행 표시 | 표시 전용 가상 재고 (DB 미기록) | 현재 설계 의도 유지 (PDF 명세 범위 외) |
