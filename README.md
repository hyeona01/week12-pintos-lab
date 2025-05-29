### DISCLAIMER: We will not use this project for Operating Systems and Lab (CS330) by Youngjin Kwon from 2025 Fall Semester.

Brand new pintos for Operating Systems and Lab (CS330), KAIST, by Youngjin Kwon.

The manual is available at https://casys-kaist.github.io/pintos-kaist/.

## 📂 Project3(Virtual Memory): 주요 구현 사항
[introduction](https://casys-kaist.github.io/pintos-kaist/project3/introduction.html)
1.	지연 로딩(Lazy Loading)
	- 프로세스 실행 시, 필요한 페이지를 즉시 로드하지 않고, 접근 시에 로드하여 메모리 사용을 최적화합니다.
2.	스택 자동 확장(Stack Growth)
	- 프로세스의 스택이 확장될 필요가 있을 때, 자동으로 새로운 페이지를 할당하여 스택을 확장합니다.
3.	파일 메모리 매핑(Memory-Mapped Files)
	- 파일의 내용을 메모리에 매핑하여, 파일 I/O를 메모리 접근으로 처리하고, 변경 사항을 파일에 반영합니다.
4.	스와핑(Swapping)
	- 물리 메모리가 부족할 경우, 사용하지 않는 페이지를 디스크의 스왑 영역으로 이동시키고, 필요 시 다시 로드합니다.
5.	보조 페이지 테이블(Supplemental Page Table)
	- 페이지 폴트 발생 시, 해당 페이지의 정보를 보조 페이지 테이블에서 조회하여 적절한 처리를 수행합니다.
6.	프레임 테이블(Frame Table)
	- 물리 메모리의 프레임 사용 현황을 추적하여, 페이지 교체 알고리즘 구현에 활용합니다.
7.	페이지 교체 알고리즘(Page Replacement Algorithm)
	- 메모리 부족 시, 교체할 페이지를 선택하는 알고리즘(예: Clock Algorithm)을 구현합니다. ￼

## 🏷️ Branch Naming Convention

- **형식**: `week{주차}-{이니셜}-{작업번호}`
- **예시**: `week09-sha-01`

## 📎 Workflow

1. **Issue Creation**  
   주차별 요구사항에 따라 개별 이슈를 생성합니다.

2. **Branch Creation**  
   네이밍 규칙(`week{주차}-{이니셜}-{작업번호}`)에 따라 브랜치를 생성하고 구현을 시작합니다.

3. **Pull Request**  
   작업이 완료되면 `main` 브랜치를 대상으로 PR을 엽니다.

4. **Code Review**  
   모든 팀원이 PR을 검토하고, 피드백이나 개선 요청을 댓글로 남깁니다.

5. **Approval**  
   최소 **2명** 이상의 팀원이 PR을 승인합니다.

6. **Merge**  
   승인이 완료되면 팀이 모여 최종적으로 PR을 대상 브랜치에 머지합니다.

## 🎯 Commit Convention

- feat : 새로운 기능 추가
- fix : 버그 수정
- docs : 문서 수정
- style : 코드 포맷팅, 세미콜론 누락, 코드 변경이 없는 경우
- refactor: 코드 리펙토링
- test: 테스트 코드, 리펙토링 테스트 코드 추가
- chore : 빌드 업무 수정, 패키지 매니저 수정
