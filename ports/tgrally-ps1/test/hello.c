/* hello.c: the toolchain, the PS-EXE and the PCDRV channel, end to end */
int bios_putchar(int c);
int pcdrv_init(void);
int pcdrv_creat(const char *name);
int pcdrv_write(int fd, const void *buf, int len);
int pcdrv_close(int fd);

static void puts_tty(const char *s)
{
    while (*s)
        bios_putchar(*s++);
}

int main(void)
{
    static const char msg[] = "hello from the PlayStation\n";
    int fd;
    puts_tty(msg);
    pcdrv_init();
    fd = pcdrv_creat("hello.txt");
    if (fd >= 0) {
        pcdrv_write(fd, msg, sizeof msg - 1);
        pcdrv_close(fd);
    }
    fd = pcdrv_creat("done");
    if (fd >= 0)
        pcdrv_close(fd);
    for (;;)
        ;
}
