#include <assert.h>
#include <circ_buf.h>
#include <stdio.h>
#ifdef TEST
int main(int argc, char **argv)
{
    circ_buf_t buf;
    char b[(1024 * 1000) + 1];
    circ_init(&buf, b, sizeof(b));
    int count = 0;
    int count2 = 0;

    for (int r = 0; r < 1000; r++) {
        printf("%d Free bytes: %zu\n", r, circ_get_free_bytes(&buf));
        for (int i = 0; i < rand() && circ_get_free_bytes(&buf) >= 3; i++) {
            char x[11];
            snprintf(x, sizeof(x), "%.10d", count++);
            circ_push(&buf, x, 10);
        }
        printf("%d Free bytes: %zu\n", r, circ_get_free_bytes(&buf));
        for (int i = 0; i < rand(); i++) {
            char x[11];
            if (circ_peek_offset(&buf, x, 10, i * 10) == 10) {
                int verify = atoi(x);
                // printf("A: %d B: %d\n", verify, count2 + i);

                assert(verify == count2 + i);
            }
        }
        for (int i = 0; i < rand() && circ_used(&buf) >= 10; i++) {
            char x[11];
            circ_pull(&buf, x, 10);
            int verify = atoi(x);
            // printf("A: %d B: %d\n", verify, count2);

            assert(verify == count2++);
        }
    }
}
#endif