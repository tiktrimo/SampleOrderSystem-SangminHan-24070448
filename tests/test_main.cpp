#include <windows.h>
#include "TestRunner.h"

// 각 CP 테스트 파일이 컴파일 단위로 포함되면 자동 등록됩니다.
// vcxproj에 test_*.cpp 파일을 추가하면 TEST() 매크로로 등록된 테스트가 자동 실행됩니다.

int main() {
    SetConsoleOutputCP(65001);
    std::cout << "=== SampleOrderSystem TDD 테스트 ===\n\n";
    return runAllTests();
}
