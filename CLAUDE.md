# CLAUDE.md

## 프로젝트 개요

반도체 시료 생산주문관리 시스템 (S-Semi 콘솔 애플리케이션)

## 아키텍처

MVC 패턴 기반 콘솔 애플리케이션

- **Model**: 도메인 객체 (Sample, Order, ProductionLine)
- **Controller**: 비즈니스 로직 처리
- **View**: 콘솔 입출력 처리
- **Repository**: JSON 파일 기반 데이터 영속성
- **Service**: 주문 승인/거절, 생산 스케줄링 등 복합 로직

## 주문 상태 Enum

```
RESERVED → CONFIRMED → RELEASE
RESERVED → PRODUCING → CONFIRMED → RELEASE
RESERVED → REJECTED
```

## 핵심 비즈니스 규칙

1. 주문 승인 시 재고가 충분하면 즉시 CONFIRMED, 부족하면 PRODUCING으로 전환
2. 생산 스케줄링은 FIFO 방식
3. 실 생산량 = ceil(부족분 / (수율 × 0.9))
4. 총 생산 시간 = 평균 생산시간 × 실 생산량
5. REJECTED 주문은 모니터링에서 제외
6. 재고 상태: 여유(충분) / 부족(주문대비 부족) / 고갈(0개)

## 코딩 규칙

- 함수/메서드는 단일 책임 원칙 준수
- 상태값은 Enum 또는 상수로 정의
- Repository는 인터페이스 기반으로 추상화
- 주문번호 형식: ORD-YYYYMMDD-NNNN

## 개발 우선순위

1. POC 검증 완료 후 본 프로젝트 구현 시작
2. 데이터 영속성: JSON 파일 방식 사용
3. 테스트 코드 작성 필수
4. Commit 메시지: feat/fix/refactor/docs/test 접두어 사용
