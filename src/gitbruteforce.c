#include <git2.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "sha1.h"
#include "tools.h"

#define THREADS 8
#define MAX_COUNTER 1000000
#define MAX_BUFFER_COUNTER 10000

void compute_commit_hash(char *str_hash, const char *raw_header, const char *commit_message)
{
  size_t raw_header_length = strlen(raw_header);
  size_t commit_message_length = strlen(commit_message);
  size_t header_length = raw_header_length + 1 + commit_message_length;

  char *data = calloc(sizeof(char), strlen("commit ") + 4 /* length */ + 1 /* null */ + header_length);
  sprintf(data, "commit %zu", header_length); // "commit LENGTH\0"
  size_t sizer_length = strlen(data) + 1;

  strncpy(&data[sizer_length], raw_header, raw_header_length);
  strncpy(&data[sizer_length + raw_header_length], "\n", 2);
  strncpy(&data[sizer_length + raw_header_length + 1], commit_message, commit_message_length);

  size_t total_length = sizer_length + strlen(&data[sizer_length]);

  SHA1_CTX sha;
  SHA1Init(&sha);
  SHA1Update(&sha, (uint8_t *)data, (uint32_t)total_length);
  SHA1Final(str_hash, &sha);

  free(data);
}

typedef struct forge_thread_params {
  pthread_mutex_t *mutex;
  const char *raw_header;
  const char *commit_message;
  size_t commit_message_length;
  const char *expected_hash;
  size_t expected_hash_length;
  int64_t begin;
  int64_t counter;
  char *found_commit_message;
  char *found_str_hash;
} forge_thread_params;

void *forge_thread(void *thread_params) {
  forge_thread_params *params = (forge_thread_params*)thread_params;

  uint64_t tid;
  pthread_threadid_np(NULL, &tid);

  int64_t clock = 0;
  int64_t buffer_counter = 0;

  char *str_hash = calloc(sizeof(char), 41);
  char *forged_commit_message = calloc(sizeof(char), params->commit_message_length + 62);
  strncpy(forged_commit_message, params->commit_message, params->commit_message_length);

  while (!params->found_commit_message && strncmp(str_hash, params->expected_hash, params->expected_hash_length))
  {
    strncpy(&forged_commit_message[params->commit_message_length], "\n", 2);
    for (int i=0;i<30;++i)
    {
      byte_to_hex_string(&forged_commit_message[params->commit_message_length+1+2*i], rand() % 256);
    }
    compute_commit_hash(str_hash, params->raw_header, forged_commit_message);

    if (++buffer_counter == MAX_BUFFER_COUNTER) {
      pthread_mutex_lock(params->mutex);
      params->counter += buffer_counter;
      buffer_counter = 0;
      if (params->counter % MAX_COUNTER == 0)
      {
        clock = time_usec() - params->begin;
        printf("[%llu] %lld H/s (%lld Mhashes in %lld usec)\n", tid, 1000000 * params->counter / clock, params->counter / 1000000, clock);
      }
      pthread_mutex_unlock(params->mutex);
    }
  }

  pthread_mutex_lock(params->mutex);
  params->counter += buffer_counter;
  if (!params->found_commit_message) {
    params->found_commit_message = forged_commit_message;
    params->found_str_hash = str_hash;
  }
  else {
    free(forged_commit_message);
    free(str_hash);
  }
  pthread_mutex_unlock(params->mutex);

  return NULL;
}

char *forge_commit_message(const git_commit *commit, const char *expected_hash)
{
  char *found_commit_message = NULL;

  forge_thread_params *params = calloc(1, sizeof(forge_thread_params));
  params->raw_header = git_commit_raw_header(commit);
  params->commit_message = git_commit_message_raw(commit);
  params->commit_message_length = strlen(params->commit_message);
  params->expected_hash = expected_hash;
  params->expected_hash_length = strlen(params->expected_hash);

  time_t t;
  srand((unsigned) time(&t));
  params->begin = time_usec();

  params->counter = 0;
  params->found_commit_message = NULL;
  params->found_str_hash = NULL;

  params->mutex = calloc(1, sizeof(pthread_mutex_t));
  if (pthread_mutex_init(params->mutex, NULL)) {
    printf("Error creating mutex\n");
    exit(1);
  }

  pthread_t *threads = calloc(THREADS, sizeof(pthread_t));

  for (int i = 0 ; i < THREADS ; ++i) {
    // printf("Creating thread %i\n", i);
    if(pthread_create(&threads[i], NULL, forge_thread, params)) {
      printf("Error creating thread\n");
      exit(1);
    }
  }
  for (int i = 0 ; i < THREADS ; ++i) {
    // printf("Joining thread %i\n", i);
    if(pthread_join(threads[i], NULL)) {
      printf("Error joining thread\n");
      exit(1);
    }
  }

  printf("%s\n", params->found_commit_message);
  int64_t clock = time_usec() - params->begin;
  printf("%s found in %lld usec (%lld tries)\n", params->found_str_hash, clock, params->counter);

  free(params->found_str_hash);
  found_commit_message = params->found_commit_message;
  params->found_commit_message = NULL;
  pthread_mutex_destroy(params->mutex);
  free(params->mutex);
  free(params);

  return found_commit_message;
}

int main(int argc, char* argv[])
{
  git_repository *repo = NULL;
  git_buf *root = NULL;
  git_reference *head = NULL;
  git_commit *last_commit = NULL;
  git_oid *parent_commit_oid = NULL;
  git_oid *new_commit_oid = NULL;
  char *forged_commit_message = NULL;

  char *repository_root = "./";
  char *expected_hash = NULL;

  if (argc == 2) {
    expected_hash = argv[1];
  }
  else {
    printf("Usage: %s <expected_hash>\n", argv[0]);
    return 0;
  }

  if (git_libgit2_init() < 1) {
    printf("Unable to init libgit2\n");
    return 0;
  }

  root = calloc(1, sizeof(git_buf));
  if (git_repository_discover(root, repository_root, 0, NULL) != 0) {
    printf("Unable to discover repository\n");
    return 0;
  }
  if (git_repository_open(&repo, root->ptr) != 0 || git_repository_is_bare(repo)) {
    printf("Unable to open repository\n");
    return 0;
  }
  free(root);

  if (git_repository_head(&head, repo) != 0) {
    printf("Unable to resolve repository HEAD\n");
    return 0;
  }

  parent_commit_oid = calloc(1, sizeof(git_oid));
  if (git_reference_name_to_id(parent_commit_oid, repo, "HEAD"))
  {
    printf("Unable to retrieve reference to HEAD\n");
    return 0;
  }
  if (git_commit_lookup(&last_commit, repo, parent_commit_oid))
  {
    printf("Unable to lookup HEAD commit\n");
    return 0;
  }
  free(parent_commit_oid);

  printf("Last commit found: %s\n", git_commit_summary(last_commit));

  forged_commit_message = forge_commit_message(last_commit, expected_hash);

  new_commit_oid = calloc(1, sizeof(git_oid));
  git_commit_amend(new_commit_oid, last_commit, "HEAD", NULL, NULL, NULL, forged_commit_message, NULL);

  free(new_commit_oid);
  free(forged_commit_message);

  git_commit_free(last_commit);
  git_reference_free(head);
  git_repository_free(repo);
  git_libgit2_shutdown();
  return 0;
}

