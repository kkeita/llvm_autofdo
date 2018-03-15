../../../../debug/bin/llvm-create-prof -profile ./inputs/input_profile.txt -profiler text -format text -binary ./inputs/find -out test_output.txt
./bin/create_llvm_prof -profile inputs/input_profile.txt -profiler text -format text -binary inputs/find -out ref_output.txt
diff ref_output.txt test_output.txt
