/*
 * Programming Assignment 02: ls-v1.2.0
 * Built on top of the instructor's base version lsv1.0.0.c
 *
 * Features so far:
 *   v1.1.0  -l   long listing format
 *   v1.2.0       default "down then across" column display  <-- added in this version
 *
 * Usage:
 *   $ ./bin/ls
 *   $ ./bin/ls -l
 *   $ ./bin/ls /etc /home
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>         // getopt(), readlink(), STDOUT_FILENO
#include <dirent.h>         // opendir(), readdir(), closedir()
#include <string.h>
#include <errno.h>
#include <limits.h>         // PATH_MAX
#include <time.h>           // ctime(), time()
#include <pwd.h>            // getpwuid()
#include <grp.h>            // getgrgid()
#include <sys/stat.h>       // stat(), lstat(), S_ISDIR() ...
#include <sys/types.h>
#include <sys/sysmacros.h>  // major(), minor() for device files
#include <sys/ioctl.h>      // ioctl(), TIOCGWINSZ, struct winsize

extern int errno;

/* ---------- display modes (which printing function to call) ---------- */
#define MODE_DEFAULT    0   /* no option: "down then across" columns */
#define MODE_LONG       1   /* -l : long listing                      */

#define SPACING        2    /* spaces between two columns             */
#define DEFAULT_WIDTH 80    /* used when terminal width is unknown    */

/* ---------- global state set by the options ---------- */
int display_mode   = MODE_DEFAULT;
int print_headers  = 0;   /* print "dirname:" above each listing? */
int listed_before  = 0;   /* used to print a blank line between listings */

/* ---------- function prototypes ---------- */
void   do_ls(const char *dir);
char **read_entries(const char *dir, int *count, int *maxlen);
void   free_entries(char **names, int count);
void   build_path(char *out, size_t size, const char *dir, const char *name);
int    get_terminal_width(void);
void   print_padded(const char *name, int width);
void   display_vertical(char **names, int n, int maxlen);
void   display_long(const char *dir, char **names, int n);
void   mode_to_string(mode_t mode, char str[11]);
void   format_time(time_t mtime, char out[13]);

int main(int argc, char *argv[])
{
    int opt;
    /* getopt() reads options one by one. "l" = the options we accept. */
    while ((opt = getopt(argc, argv, "l")) != -1)
    {
        switch (opt)
        {
        case 'l': display_mode = MODE_LONG;       break;
        default:  /* getopt already printed "invalid option" */
            fprintf(stderr, "Usage: %s [-l] [directory...]\n", argv[0]);
            exit(1);
        }
    }

    /* After getopt(), argv[optind] .. argv[argc-1] are the directory names */
    int num_dirs = argc - optind;

    /* Like real ls: show "dirname:" headers when listing more than one
       directory */
    if (num_dirs > 1)
        print_headers = 1;

    if (num_dirs == 0)
        do_ls(".");
    else
        for (int i = optind; i < argc; i++)
            do_ls(argv[i]);

    return 0;
}

/*
 * do_ls: read all names -> display them
 */
void do_ls(const char *dir)
{
    if (print_headers)
    {
        if (listed_before)
            printf("\n");          /* blank line between two listings */
        printf("%s:\n", dir);
    }
    listed_before = 1;

    int n, maxlen;
    char **names = read_entries(dir, &n, &maxlen);
    if (names == NULL)
        return;

    /* call the right display function based on the mode flag */
    if (display_mode == MODE_LONG)
        display_long(dir, names, n);
    else
        display_vertical(names, n, maxlen);

    free_entries(names, n);
}

/*
 * read_entries: reads all (non-hidden) names of a directory into a
 * dynamically growing array of strings. Also finds the longest name.
 */
char **read_entries(const char *dir, int *count, int *maxlen)
{
    DIR *dp = opendir(dir);
    if (dp == NULL)
    {
        fprintf(stderr, "Cannot open directory : %s\n", dir);
        return NULL;
    }

    int capacity = 16;           /* start with room for 16 names */
    int n = 0, longest = 0;
    char **names = malloc(capacity * sizeof(char *));
    if (names == NULL)
    {
        perror("malloc failed");
        exit(1);
    }

    struct dirent *entry;
    errno = 0;
    while ((entry = readdir(dp)) != NULL)
    {
        if (entry->d_name[0] == '.')   /* skip hidden files (same as base code) */
            continue;

        if (n == capacity)             /* array is full -> double its size */
        {
            capacity *= 2;
            char **tmp = realloc(names, capacity * sizeof(char *));
            if (tmp == NULL)
            {
                perror("realloc failed");
                exit(1);
            }
            names = tmp;
        }
        names[n] = strdup(entry->d_name);   /* copy the name (malloc + strcpy) */
        int len = strlen(entry->d_name);
        if (len > longest)
            longest = len;
        n++;
    }

    if (errno != 0)
        perror("readdir failed");

    closedir(dp);
    *count = n;
    *maxlen = longest;
    return names;
}

void free_entries(char **names, int count)
{
    for (int i = 0; i < count; i++)
        free(names[i]);     /* free each string ...   */
    free(names);            /* ... then the array itself */
}

/* build_path: "dir" + "/" + "name"  ->  e.g. "src/obj" */
void build_path(char *out, size_t size, const char *dir, const char *name)
{
    size_t len = strlen(dir);
    if (len > 0 && dir[len - 1] == '/')   /* dir already ends with '/' */
        snprintf(out, size, "%s%s", dir, name);
    else
        snprintf(out, size, "%s/%s", dir, name);
}

/* get_terminal_width: ask the terminal driver how many columns it has */
int get_terminal_width(void)
{
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0)
        return DEFAULT_WIDTH;     /* not a terminal (e.g. output piped) */
    return ws.ws_col;
}

/* print_padded: print the name, then spaces until "width" characters */
void print_padded(const char *name, int width)
{
    printf("%s", name);
    for (int i = strlen(name); i < width; i++)
        putchar(' ');
}

/*
 * display_vertical: default ls output, "down then across".
 * Item i is placed in row (i % rows) and column (i / rows), so while
 * printing row r we print items r, r+rows, r+2*rows, ...
 */
void display_vertical(char **names, int n, int maxlen)
{
    if (n == 0)
        return;

    int width     = get_terminal_width();
    int col_width = maxlen + SPACING;
    int cols      = width / col_width;
    if (cols < 1)
        cols = 1;
    int rows = (n + cols - 1) / cols;     /* ceiling of n / cols */

    for (int r = 0; r < rows; r++)
    {
        for (int c = 0; c < cols; c++)
        {
            int i = r + c * rows;         /* index of the item at (r, c) */
            if (i >= n)
                break;
            if (i + rows < n)             /* another item comes after it on this row */
                print_padded(names[i], col_width);
            else                          /* last item of the row: no padding */
                printf("%s", names[i]);
        }
        printf("\n");
    }
}

/*
 * mode_to_string: convert st_mode into "drwxr-xr-x" form,
 * including the special bits (setuid, setgid, sticky).
 */
void mode_to_string(mode_t mode, char str[11])
{
    /* 1st character: the file type */
    if      (S_ISDIR(mode))  str[0] = 'd';
    else if (S_ISLNK(mode))  str[0] = 'l';
    else if (S_ISCHR(mode))  str[0] = 'c';
    else if (S_ISBLK(mode))  str[0] = 'b';
    else if (S_ISFIFO(mode)) str[0] = 'p';
    else if (S_ISSOCK(mode)) str[0] = 's';
    else                     str[0] = '-';

    /* owner, group, others: test each permission bit with & */
    str[1] = (mode & S_IRUSR) ? 'r' : '-';
    str[2] = (mode & S_IWUSR) ? 'w' : '-';
    str[3] = (mode & S_IXUSR) ? 'x' : '-';
    str[4] = (mode & S_IRGRP) ? 'r' : '-';
    str[5] = (mode & S_IWGRP) ? 'w' : '-';
    str[6] = (mode & S_IXGRP) ? 'x' : '-';
    str[7] = (mode & S_IROTH) ? 'r' : '-';
    str[8] = (mode & S_IWOTH) ? 'w' : '-';
    str[9] = (mode & S_IXOTH) ? 'x' : '-';

    /* special bits: lowercase if x is also set, uppercase if not */
    if (mode & S_ISUID) str[3] = (mode & S_IXUSR) ? 's' : 'S';
    if (mode & S_ISGID) str[6] = (mode & S_IXGRP) ? 's' : 'S';
    if (mode & S_ISVTX) str[9] = (mode & S_IXOTH) ? 't' : 'T';

    str[10] = '\0';
}

/*
 * format_time: ctime() gives "Wed Jun 30 21:49:08 1993\n".
 * Real ls shows "Jun 30 21:49" for recent files, and
 * "Jun 30  1993" for files older than ~6 months (or in the future).
 */
#define SIX_MONTHS (31556952 / 2)   /* half of an average year, in seconds */

void format_time(time_t mtime, char out[13])
{
    char *t = ctime(&mtime);
    time_t now = time(NULL);

    if (mtime <= now && now - mtime < SIX_MONTHS)
        snprintf(out, 13, "%.12s", t + 4);           /* "Jun 30 21:49" */
    else
        snprintf(out, 13, "%.6s  %.4s", t + 4, t + 20); /* "Jun 30  1993" */
}

/* helpers that turn uid / gid / size into printable text */
static void get_owner(uid_t uid, char *buf, size_t size)
{
    struct passwd *pwd = getpwuid(uid);
    if (pwd != NULL) snprintf(buf, size, "%s", pwd->pw_name);
    else             snprintf(buf, size, "%u", (unsigned)uid);   /* no name: show number */
}

static void get_group(gid_t gid, char *buf, size_t size)
{
    struct group *grp = getgrgid(gid);
    if (grp != NULL) snprintf(buf, size, "%s", grp->gr_name);
    else             snprintf(buf, size, "%u", (unsigned)gid);
}

/* number of digits needed to print a number, e.g. 4096 -> 4 */
static int num_width(unsigned long x)
{
    return snprintf(NULL, 0, "%lu", x);
}

/*
 * display_long: -l output. Two passes:
 *   pass 1: lstat() every file, find the widest value of each column
 *   pass 2: print every line using those widths so columns line up
 */
void display_long(const char *dir, char **names, int n)
{
    struct stat *info = malloc((n > 0 ? n : 1) * sizeof(struct stat));
    int *ok = malloc((n > 0 ? n : 1) * sizeof(int));
    if (info == NULL || ok == NULL)
    {
        perror("malloc failed");
        exit(1);
    }

    char path[PATH_MAX], owner[64], group[64];
    int w_links = 0, w_owner = 0, w_group = 0, w_size = 0, len;
    int w_major = 0, w_minor = 0, has_device = 0;
    long total_blocks = 0;

    /* ---- pass 1: collect metadata and column widths ---- */
    for (int i = 0; i < n; i++)
    {
        build_path(path, sizeof(path), dir, names[i]);
        ok[i] = (lstat(path, &info[i]) == 0);
        if (!ok[i])
        {
            perror(path);
            continue;
        }
        total_blocks += info[i].st_blocks;

        len = num_width(info[i].st_nlink);
        if (len > w_links) w_links = len;

        get_owner(info[i].st_uid, owner, sizeof(owner));
        len = strlen(owner);
        if (len > w_owner) w_owner = len;

        get_group(info[i].st_gid, group, sizeof(group));
        len = strlen(group);
        if (len > w_group) w_group = len;

        if (S_ISCHR(info[i].st_mode) || S_ISBLK(info[i].st_mode))
        {   /* device files show "major, minor" instead of a size */
            has_device = 1;
            len = num_width(major(info[i].st_rdev));
            if (len > w_major) w_major = len;
            len = num_width(minor(info[i].st_rdev));
            if (len > w_minor) w_minor = len;
        }
        else
        {
            len = num_width(info[i].st_size);
            if (len > w_size) w_size = len;
        }
    }

    /* the size column must also be wide enough for "major, minor" */
    if (has_device && w_major + 2 + w_minor > w_size)
        w_size = w_major + 2 + w_minor;

    /* st_blocks counts 512-byte blocks; ls reports 1024-byte blocks */
    printf("total %ld\n", total_blocks / 2);

    /* ---- pass 2: print each line ---- */
    char perms[11], timestr[13];
    for (int i = 0; i < n; i++)
    {
        if (!ok[i])
            continue;

        mode_to_string(info[i].st_mode, perms);
        get_owner(info[i].st_uid, owner, sizeof(owner));
        get_group(info[i].st_gid, group, sizeof(group));
        format_time(info[i].st_mtime, timestr);

        printf("%s %*lu %-*s %-*s ",
               perms,
               w_links, (unsigned long)info[i].st_nlink,
               w_owner, owner,
               w_group, group);

        if (S_ISCHR(info[i].st_mode) || S_ISBLK(info[i].st_mode))
            printf("%*u, %*u ",
                   w_size - 2 - w_minor, major(info[i].st_rdev),
                   w_minor, minor(info[i].st_rdev));
        else
            printf("%*ld ", w_size, (long)info[i].st_size);

        printf("%s ", timestr);
        printf("%s", names[i]);

        /* symbolic link: also show where it points, e.g. "link -> target" */
        if (S_ISLNK(info[i].st_mode))
        {
            char target[PATH_MAX];
            build_path(path, sizeof(path), dir, names[i]);
            ssize_t tlen = readlink(path, target, sizeof(target) - 1);
            if (tlen != -1)
            {
                target[tlen] = '\0';
                printf(" -> %s", target);
            }
        }
        printf("\n");
    }

    free(info);
    free(ok);
}
