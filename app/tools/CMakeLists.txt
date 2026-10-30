# Desktop-only developer tools; not registered with ctest.
add_executable(recognize-cli recognize-cli.cpp)
target_link_libraries(recognize-cli PRIVATE whilefresh-recognition whilefresh-ai)

add_executable(recipe-cli recipe-cli.cpp)
target_link_libraries(recipe-cli PRIVATE whilefresh-recipes)

# recognize-cli refuses to record into tracked parts of the repository. In a git worktree the
# main checkout is a second root.
get_filename_component(_repo_root "${PROJECT_SOURCE_DIR}/.." ABSOLUTE)
set(_repo_roots "${_repo_root}")
find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" -C "${_repo_root}" rev-parse --path-format=absolute --git-common-dir
        OUTPUT_VARIABLE _git_common_dir
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE _git_result
    )
    if(_git_result EQUAL 0 AND _git_common_dir)
        get_filename_component(_main_root "${_git_common_dir}/.." ABSOLUTE)
        list(APPEND _repo_roots "${_main_root}")
        list(REMOVE_DUPLICATES _repo_roots)
    endif()
endif()
list(JOIN _repo_roots "|" _repo_roots_joined)
target_compile_definitions(recognize-cli PRIVATE WHILEFRESH_REPO_ROOTS="${_repo_roots_joined}")
