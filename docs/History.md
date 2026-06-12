# Project History

## Session Log — 2026-06-12

---

| # | Prompt | Result |
|---|--------|--------|
| 1 | PDF 분석 및 POC 폴더 구조 생성, GitHub 레포 생성 요청 | PDF 분석 완료. 5개 레포 생성(`ConsoleMVC`, `DataPersistence`, `DataMonitor`, `DummyDataGenerator`, `SampleOrderSystem`) 및 초기 커밋 push |
| 2 | git credential 사용, 이름 SangminHan / 사번 24070448 지정 | GitHub 계정(tiktrimo) 확인, 올바른 이름/사번으로 레포 재생성 |
| 3 | POC 단계임을 커밋에 명시 요청 | `poc:` 접두어 커밋 추가, README에 `[POC Stage]` 배너 삽입 후 push |
| 4 | 개발 언어 확인 | 사용자가 C++ with Visual Studio 선택 |
| 5 | C++ Visual Studio 프로젝트로 설정 | `.gitignore` VS용으로 교체, README·CLAUDE.md C++ 기준 업데이트, `/utf-8` 컴파일 플래그 설정 |
| 6 | 4개 POC 순서대로 구현 요청 | POC별 기능 단위 커밋(총 22회)으로 전체 구현 완료 — MVC 스켈레톤 / 파일 CRUD / 모니터링 / 더미 데이터 생성기 |
| 7 | 빌드 및 테스트 요청 | MSBuild Debug\|x64 — 4개 모두 에러 없이 빌드·실행 성공. 상태 전환·영속성·모니터링·데이터 생성 동작 검증 |
| 8 | 각 프롬프트와 결과를 History.md로 저장 요청 | 본 파일 생성 |
