# CLAUDE.md

## 프로젝트 개요

반도체 시료 생산주문관리 시스템 (S-Semi 콘솔 애플리케이션)

## 개발 환경

- **언어**: C++17
- **IDE**: Visual Studio 2022 (v143 toolset)
- **빌드**: MSBuild Debug|x64, `/utf-8` 플래그
- **데이터 저장**: 파이프 구분 텍스트 파일 (`data/` 디렉터리)

## 아키텍처

MVC 패턴 기반 콘솔 애플리케이션

- **Model** (`src/model/`): 도메인 객체 — `Sample`, `Order`
- **Controller** (`src/controller/`): 비즈니스 로직 처리
- **View** (`src/view/`): 콘솔 입출력 처리
- **Repository** (`src/repository/`): 파일 기반 데이터 영속성
- **Service** (`src/service/`): 주문 승인/거절, 생산 스케줄링 등 복합 로직

## 주문 상태 Enum

```cpp
enum class OrderStatus {
    RESERVED,   // 주문 접수
    REJECTED,   // 주문 거절
    PRODUCING,  // 생산 중 (재고 부족)
    CONFIRMED,  // 출고 대기
    RELEASE     // 출고 완료
};
```

## 핵심 비즈니스 규칙

1. 주문 승인 시 재고 ≥ 주문량 → CONFIRMED, 재고 < 주문량 → PRODUCING
2. PRODUCING 전환 시 재고 전량 차감, 생산 큐(FIFO)에 자동 등록
3. 실 생산량 = `ceil(부족분 / (수율 × 0.9))`
4. 총 생산 시간 = `평균 생산시간 × 실 생산량`
5. REJECTED 주문은 모니터링에서 제외
6. 재고 상태: 여유(stock≥pending) / 부족(stock<pending, stock>0) / 고갈(stock==0)
7. 주문번호 형식: `ORD-YYYYMMDD-NNNN`

## 코딩 규칙

- 헤더(`.h`)와 소스(`.cpp`) 파일 분리; 클래스 선언은 `.h`, 구현은 `.cpp`
- 상태값은 `enum class`로 정의
- Repository는 `IRepository<T>` 인터페이스 기반으로 추상화
- 메모리 관리: 스마트 포인터(`std::unique_ptr`, `std::shared_ptr`) 사용 권장
- 파일 포맷: 파이프 구분 (`field1|field2|...`), POC 레포와 호환
- 런타임 초기화: `SetConsoleOutputCP(65001)` (한국어 출력)

---

## TDD 워크플로우 (Red-Green 커밋 패턴)

**이 패턴을 반드시 지켜 커밋 이력을 풍부하게 만든다.**

### 매 체크포인트 커밋 순서

```
test: [CPn] <기능명> — 실패 테스트 작성       ← RED  (테스트 FAIL)
feat: [CPn] <기능명> — 구현                   ← GREEN (테스트 PASS)
refactor: [CPn] <기능명> — 정리               ← (선택, 로직 변경 없이)
chore: [CPn] checkpoint complete              ← 체크포인트 완료
```

- RED 커밋 조건: `SampleOrderSystemTests.exe` 실행 시 `[FAIL]` 출력
- GREEN 커밋 조건: 해당 CP의 모든 테스트 `[PASS]` 출력
- 한 번에 하나의 기능씩 RED → GREEN 사이클

### 빌드 명령

```
"C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe" SampleOrderSystem.sln /p:Configuration=Debug /p:Platform=x64 /verbosity:minimal /nologo
```

### 테스트 실행

```
x64\Debug\SampleOrderSystemTests.exe
```

### 빌드 + 테스트 (Hook이 자동 실행)

```
"C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe" SampleOrderSystem.sln /p:Configuration=Debug /p:Platform=x64 /verbosity:minimal /nologo && x64\Debug\SampleOrderSystemTests.exe
```

---

## PRD 체크포인트

### CP1: 시료 관리
- [ ] `Sample` 모델: `id`, `name`, `avgProductionTimeMin`, `yield`, `stock`
- [ ] `IRepository<Sample>` 인터페이스
- [ ] `SampleRepository`: 파일 CRUD (`data/samples.dat`, 파이프 구분)
- [ ] `SampleController`: `addSample`, `getAll`, `findById`, `searchByName`, `updateStock`
- **AC**: 등록 후 `findById` 반환 확인, 이름 부분 검색 결과 반환, 재고 업데이트 반영

### CP2: 주문 접수
- [ ] `Order` 모델: `orderId`, `sampleId`, `customerName`, `quantity`, `status`
- [ ] `IRepository<Order>` 인터페이스
- [ ] `OrderRepository`: 파일 CRUD + `findByStatus` (`data/orders.dat`)
- [ ] `OrderController::placeOrder` → RESERVED, `ORD-YYYYMMDD-NNNN` 형식
- **AC**: 상태 RESERVED 확인, 주문번호 `ORD-`로 시작 검증

### CP3: 주문 승인/거절
- [ ] `OrderService::approve`: stock≥qty → CONFIRMED + 재고차감, stock<qty → PRODUCING + 재고전량차감 + 큐등록
- [ ] `OrderService::reject` → REJECTED
- **AC**: 재고 충분 → CONFIRMED, 재고 부족 → PRODUCING, 거절 → REJECTED, 재고 차감 검증

### CP4: 생산라인
- [ ] `ProductionQueue`: FIFO 큐 (`std::queue` 기반)
- [ ] `ProductionService::calculate`: `ceil(부족분 / (수율 × 0.9))`
- [ ] `ProductionService::complete`: PRODUCING → CONFIRMED, 재고 증가
- **AC**: 공식 정확성, FIFO 순서 보장, 완료 후 상태 CONFIRMED

### CP5: 모니터링
- [ ] `DataAggregator`: 상태별 주문건수 (REJECTED 제외), 재고상태 분류
- [ ] `ConsoleMonitor`: 주문현황 + 재고현황 콘솔 출력
- **AC**: REJECTED 집계 제외, stock==0 → 고갈, stock<pending합산 → 부족, 나머지 → 여유

### CP6: 출고 처리
- [ ] `OrderService::release`: CONFIRMED → RELEASE
- [ ] View: CONFIRMED 목록 표시, 선택 후 출고 실행
- **AC**: CONFIRMED만 출고 가능, RELEASE 전환 확인, 다른 상태에서 오류 처리

---

## 하네스 규칙

- `.claude/settings.json`의 `PostToolUse` Hook이 `.cpp`/`.h` 파일 수정 후 자동으로 빌드+테스트를 실행합니다
- Hook 결과(빌드 에러 또는 테스트 FAIL)가 보이면 즉시 수정하고 다시 저장합니다
- vcxproj에 새 `.cpp` 파일 추가 시 반드시 `<ClCompile>` 항목도 함께 추가합니다
- 체크포인트 완료 기준: 해당 CP의 모든 AC를 만족하는 테스트가 `[PASS]`
