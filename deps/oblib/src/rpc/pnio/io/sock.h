#include <sys/epoll.h>
struct sock_t;
#define SOCK_FACTORY_COMMON                       \
  struct sock_t* (*create)(struct sf_t*);         \
  void (*destroy)(struct sf_t*, struct sock_t*);

typedef struct sf_t {
  SOCK_FACTORY_COMMON;
} sf_t;

typedef int (*handle_event_t)(struct sock_t*);
#define SOCK_COMMON                             \
  struct sf_t* fty;                             \
  handle_event_t handle_event;                  \
  dlink_t ready_link;                           \
  int fd;                                       \
  uint32_t mask;                                \
  uint8_t conn_ok:1

typedef struct sock_t {
  SOCK_COMMON;
} sock_t;


#define EPOLLPENDING EPOLLONESHOT
inline void skset(sock_t* s, uint32_t m) { s->mask |= m; }
inline void skclear(sock_t* s, uint32_t m){ s->mask &= ~m; }
inline bool sktest(sock_t* s, uint32_t m) { return s->mask & m; }
#define sks(s, flag) skset((sock_t*)s, EPOLL ## flag)
#define skt(s, flag) sktest((sock_t*)s, EPOLL ## flag)
#define skc(s, flag) skclear((sock_t*)s, EPOLL ## flag)

extern void sf_init(sf_t* sf, void* create, void* destroy);
extern void sk_init(sock_t* s, sf_t* sf, void* handle_event, int fd);
