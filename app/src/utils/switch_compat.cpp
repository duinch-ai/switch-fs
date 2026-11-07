#if defined(__SWITCH__)
#include <sys/types.h>
#include <sys/stat.h>

struct passwd;
struct group;

extern "C" {

mode_t umask(mode_t mask) {
    (void)mask;
    return 0022;
}

struct passwd *getpwnam(const char *name) {
    (void)name;
    return nullptr;
}

struct group *getgrnam(const char *name) {
    (void)name;
    return nullptr;
}

struct passwd *getpwuid(uid_t uid) {
    (void)uid;
    return nullptr;
}

struct group *getgrgid(gid_t gid) {
    (void)gid;
    return nullptr;
}

} // extern "C"
#endif
