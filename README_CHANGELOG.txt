Wordfish 1.0.1 220825 hotfix
- Fix: evaluate.cpp trace() now defines posKey before use.
-Fix: ucioption.cpp — no functional change needed (parser already OK)Your setoption parser correctly handles names and values with spaces (e.g., “Book1 File”, paths with spaces). No change required here. 
- Note: build with Clang++. 
