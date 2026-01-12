#define _POSIX_C_SOURCE 200809L
#include <string.h>
#include <asm-generic/errno-base.h>
#include <sys/time.h>
#include <asm-generic/socket.h>
#include <bits/types/struct_iovec.h>
#include <bits/types/struct_timeval.h>
#include <sys/socket.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <bits/getopt_core.h>
#include <netinet/in_systm.h>
#include <netinet/ip_icmp.h>
#include <signal.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/ip.h>

#define BUFSIZE 1500

#ifdef IPV6
# include <netinet/ip6.h>
# include <netinet/icmp6.h>
#endif

struct proto {
  void (*procf)(char*, ssize_t, struct msghdr*, struct timeval*);
  void (*sendf)(void);
  void (*initf)(void);
  struct sockaddr* sa_send;
  struct sockaddr* sa_recv;
  socklen_t sa_len;
  int icmp_proto;
};

pid_t pid = -1;
int sockfd = -1;
struct proto* pr = nullptr;
char sendbuf[BUFSIZE] = {};
char p_buf[INET6_ADDRSTRLEN] = {};
size_t nseq = 0;
size_t datalen = 56;

typedef void(*sigfunc)(int);

sigfunc signal(int signo, sigfunc handler) {
  struct sigaction act = {
    .sa_handler = handler,
    .sa_mask = {},
    .sa_flags = 0,
  };

  if (signo == SIGALRM) {
    #ifdef SA_INTERRUPT
      act.sa_flags |= SA_INTERRUPT; /* SunOS 4 */
      /* Disables automatic restart of interrupted system call */
    #endif
  }
  else {
    #ifdef SA_RESTART
      act.sa_flags |= SA_RESTART; /* SVR4, 44BSD */
    #else
      #warning "SA_RESTART not defined"
    #endif
  }

  struct sigaction oact;
  if (sigaction(signo, &act, &oact) < 0) {
    return SIG_ERR;
  }

  return oact.sa_handler;
}
sigfunc Signal(int signo, sigfunc handler) {
  sigfunc r = signal(signo, handler);
  if (r == SIG_ERR) {
    perror("signal() error");
    exit(EXIT_FAILURE);
  }
  return r;
}

struct addrinfo* host_serv(const char* hostname, const char* service, int family, int socktype) {
  struct addrinfo hints = {
    .ai_family = family,
    .ai_socktype = socktype,
    .ai_flags = AI_CANONNAME
  };

  struct addrinfo* res = nullptr;
  int err = getaddrinfo(hostname, service, &hints, &res);
  if (err != 0) {
    return (void*)((intptr_t)err);
  }

  return res;
}
struct addrinfo* Host_serv(const char* hostname, const char* service, int family, int socktype) {
  struct addrinfo* r = host_serv(hostname, service, family, socktype);
  if (r == nullptr) {
    fprintf(stderr, "getaddrinfo() error: %s", gai_strerror((intptr_t)r));
    exit(EXIT_FAILURE);
  }
  return r;
}

void* Calloc(size_t n, size_t size) {
  void* r = calloc(n, size);
  if (r == nullptr) {
    perror("calloc() error");
    exit(EXIT_FAILURE);
  }
  return r;
}

int Socket(int domain, int type, int protocol) {
  int r = socket(domain, type, protocol);
  if (r == -1) {
    perror("socket() error");
    exit(EXIT_FAILURE);
  }
  return r;
}

void Gettimeofday(struct timeval *restrict tv, void *restrict tz) {
  int err = gettimeofday(tv, tz);
  if (err == -1) {
    perror("gettimeofdat() error");
    exit(EXIT_FAILURE);
  }
}

uint16_t in_cksum(uint16_t* addr, int len) {
  int nleft = len;
  uint16_t* w = addr;
  uint32_t sum = 0;

  while (nleft > 1) {
    sum += *w++;
    nleft -= 2;
  }

  uint16_t answer = 0;
  if (nleft == 1) {
    *(unsigned char*)(&answer) = *(unsigned char*)w;
    sum += answer;
  }

  sum  = (sum >> 16) + (sum & 0xffff);
  sum += (sum >> 16);
  answer = ~sum;

  return answer;
}

void sig_alarm(int signo) {
  (void)signo;

  (*pr->sendf)();

  alarm(1);
  return;
}

void tv_sub(struct timeval* out, struct timeval* in) {
  if ((out->tv_usec -= in->tv_usec) < 0) {
    --out->tv_sec;
    out->tv_usec += 1'000'000;
  }
  out->tv_sec -= in->tv_sec;
}

void proc_v4(char* ptr, ssize_t len, struct msghdr* msg, struct timeval* tvrecv) {
  (void)msg;

  // struct ip* ip = (struct ip*)ptr;
  struct iphdr* ip = (struct iphdr*)ptr;
  size_t iphlen = ip->ihl << 2;
  if (ip->protocol != IPPROTO_ICMP) {
    return;
  }

  struct icmphdr* icmp = (struct icmphdr*)(ptr + iphlen);
  size_t icmplen = len;
  if (icmplen - iphlen < 8) {
    return;
  }

  if (icmp->type == ICMP_ECHOREPLY) {
    if (icmp->un.echo.id != pid) {
      return;
    }
    if (icmplen < 16) {
      return;
    }

    // struct timeval* tvsend = (struct timeval*)(icmp + 1);
    struct timeval tvsend = {};
    memcpy(&tvsend, icmp + 1, sizeof(struct timeval));
    tv_sub(tvrecv, &tvsend);
    double rtt  = tvrecv->tv_sec * 1000.0 + tvrecv->tv_usec / 1000.0;

    // char p_buf[INET_ADDRSTRLEN] = {};
    // inet_ntop(AF_INET, pr->sa_recv, p_buf, sizeof(p_buf));

    printf("%lu bytes from %s: seq=%u, ttl=%d, rtt=%.3f ms\n", icmplen, p_buf, icmp->un.echo.sequence, ip->ttl, rtt);
  }
}
void send_v4() {
  struct icmphdr* icmp = (struct icmphdr*)sendbuf;
  icmp->type = ICMP_ECHO;
  icmp->code = 0;
  icmp->un.echo.id = pid;
  icmp->un.echo.sequence = nseq++;

  memset(sendbuf + sizeof(struct icmphdr), 0xa5, datalen);

  Gettimeofday((struct timeval*)(sendbuf + sizeof(struct icmphdr)), nullptr);

  size_t len = 8 + datalen;

  icmp->checksum = 0;
  icmp->checksum = in_cksum((uint16_t*)icmp, len);
  sendto(sockfd, sendbuf, len, 0, pr->sa_send, pr->sa_len);
}

void proc_v6(char* ptr, ssize_t len, struct msghdr* msg, struct timeval* tvrecv) {
  (void)ptr;
  (void)len;
  (void)msg;
  (void)tvrecv;
  #ifdef IPV6



  #endif
}
void send_v6() {
  #ifdef IPV6

  #endif
}
void init_v6() {
  #ifdef IPV6
  if () {

  }
  #ifdef IPV6_RECVHOPLIMIT

  #else

  #endif
  #endif
}

struct proto proto_v4 = {
  proc_v4, send_v4, nullptr, nullptr, nullptr, 0, IPPROTO_ICMP 
};

#ifdef IPV6
struct proto proto_v6 = {
  proc_v6, send_v6, init_v6, nullptr, nullptr, 0, IPPROTO_ICMPV6
};
#endif


void usage() {
  fprintf(stderr, "usage: ping [ -v ] hostname\n");
}

void readloop() {
  sockfd = Socket(pr->sa_send->sa_family, SOCK_RAW, pr->icmp_proto);
  setuid(getuid());

  if (pr->initf != nullptr) {
    (*pr->initf)();
  }

  const size_t new_buf_size = 60 * 1024;
  int err = setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &new_buf_size, sizeof(new_buf_size));
  if (err == -1) {
    perror("setsockopt() error");
    errno = 0;
  }

  sig_alarm(SIGALRM);

  char recvbuf[BUFSIZ] = {};
  char controlbuf[BUFSIZ] = {};

  struct iovec iov = {
    .iov_base = recvbuf,
    .iov_len = sizeof(recvbuf),
  };

  struct msghdr msg = {
    .msg_name = pr->sa_recv,
    .msg_iov = &iov,
    .msg_iovlen = 1,
    .msg_control = controlbuf
  };

  for (;;) {
    msg.msg_namelen = pr->sa_len;
    msg.msg_controllen = sizeof(controlbuf);

    int nbytes = recvmsg(sockfd, &msg, 0);
    if (nbytes == -1) {
      if (errno == EINTR) {
        continue;
      } 
      else {
        perror("recvmsg() error");
        exit(EXIT_FAILURE);
      }
    }

    struct timeval tv = {};
    Gettimeofday(&tv, nullptr);
    (*pr->procf)(recvbuf, nbytes, &msg, &tv);
  }
}

int main(int argc, char* argv[]) {
  opterr = 0;
  char c = -1;
  while ((c = getopt(argc, argv, "v")) != -1) {
    switch (c) {
      case 'v': 
        printf("verbose++");
        break;
      case '?': 
        printf("unrecognized option\n");
        exit(EXIT_FAILURE);
    }
  }
  printf("\n");

  if (optind != argc - 1) {
    usage();
    exit(EXIT_FAILURE);
  }

  const char* host = argv[optind];
  printf("host = %s\n", host);

  pid = getpid();
  Signal(SIGALRM, sig_alarm);

  struct addrinfo* ai = Host_serv(host, nullptr, 0, 0);
  if (ai == nullptr) {
    perror("host resolve error");
    exit(EXIT_FAILURE);
  }
  
  
  // if (inet_ntop(ai->ai_family, ai->ai_addr, buf, sizeof(buf)) == nullptr) {
  //   perror("inet_ntop error");
  //   errno = 0;
  // }
  
  if (ai->ai_family == AF_INET) {
    if (inet_ntop(ai->ai_family, &((struct sockaddr_in*)ai->ai_addr)->sin_addr, p_buf, sizeof(p_buf)) == nullptr) {
      perror("inet_ntop error");
      errno = 0;
    }
    
    pr = &proto_v4;
  #ifdef IPV6
  } 
  else if (ai->ai_family == AF_INET6) {
    if (inet_ntop(ai->ai_family, &((struct sockaddr_in6*)ai->ai_addr)->sin6_addr, p_buf, sizeof(p_buf)) == nullptr) {
      perror("inet_ntop error");
      errno = 0;
    }

    pr = &proto_v6;
    struct in6_addr* addr6 = &(((struct sockaddr_in6*)ai->ai_addr)->sin6_addr);
    if (IN6_IS_ADDR_V4MAPPED( addr6 )) {
      fprintf(stderr,  "cannot ping IPv4-mapped IPv6 address");
      exit(EXIT_FAILURE);
    }
  #endif
  } 
  else {
    fprintf(stderr, "unknown address family %d", ai->ai_family);
    exit(EXIT_FAILURE);
  }

  printf("PING %s : %s\n", ai->ai_canonname ? ai->ai_canonname : host, p_buf);

  pr->sa_send = ai->ai_addr;
  pr->sa_len = ai->ai_addrlen;
  pr->sa_recv = Calloc(1, pr->sa_len);

  readloop();

  freeaddrinfo(ai);
  return 0;
}
