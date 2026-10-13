# OS Programming Assignment 02 — Report

**Name:** Mutahhar Ahmad
**Roll No:** BSDSF24M028
**Repository:** BSDSF24M028-OS-A02

---

## Feature 2 — ls-v1.1.0: Long Listing Format

### Q1. What is the crucial difference between the `stat()` and `lstat()` system calls? In the context of the `ls` command, when is it more appropriate to use `lstat()`?

Both system calls fill a `struct stat` with a file's metadata (type, permissions, link count, owner, group, size, timestamps). The difference is how they treat **symbolic links**:

- `stat()` **follows** the link and returns information about the file the link points to (the target).
- `lstat()` does **not** follow the link and returns information about the link itself (its type is `S_IFLNK` and its size is the length of the path stored in it).

For a normal file both give the same result.

`ls -l` must show a symbolic link as a link (`l` as the first character and `name -> target` at the end), so `lstat()` is the right choice. If `stat()` were used, every link would look like its target, and a link whose target was deleted (a broken link) would cause an error. `lstat()` is also important in recursive listing (`-R`): it prevents the program from following a link that points to a directory, which could otherwise cause infinite loops.

### Q2. The `st_mode` field in `struct stat` is an integer that contains both the file type and the permission bits. Explain how you can use bitwise operators (like `&`) and predefined macros (like `S_IFDIR` or `S_IRUSR`) to extract this information.

`st_mode` packs several pieces of information into different bits of one integer. In octal, for example, `0100664` means:

```
1000      000        110     110     100
type      special    owner   group   others
(regular) (s s t)    r w -   r w -   r - -
```

The bitwise AND operator `&` keeps only the bits that are 1 in **both** numbers, so it can be used as a **mask** to test one bit at a time:

- **Permission bits:** `S_IRUSR` is `0400` (owner read). `mode & S_IRUSR` is non-zero if that bit is on, and zero if it is off:
  ```c
  str[1] = (mode & S_IRUSR) ? 'r' : '-';
  ```
  The same is done with `S_IWUSR`, `S_IXUSR`, `S_IRGRP`, `S_IWGRP`, `S_IXGRP`, `S_IROTH`, `S_IWOTH`, `S_IXOTH`.

- **File type:** the type uses 4 bits together, so we mask all of them with `S_IFMT` (`0170000`) and compare the result with a type constant:
  ```c
  if ((mode & S_IFMT) == S_IFDIR)   /* directory */
  ```
  The macro `S_ISDIR(mode)` is a shortcut for exactly this. Similar macros are `S_ISREG`, `S_ISLNK`, `S_ISCHR`, `S_ISBLK`, `S_ISFIFO` and `S_ISSOCK`.

- **Special bits:** `S_ISUID` (`04000`, setuid), `S_ISGID` (`02000`, setgid) and `S_ISVTX` (`01000`, sticky). They are shown as `s`/`s`/`t` in place of the owner/group/others `x` when that `x` bit is also set, and as uppercase `S`/`S`/`T` when it is not.

---

## Feature 3 — ls-v1.2.0: Column Display (Down Then Across)

### Q3. Explain the general logic for printing items in a "down then across" columnar format. Why is a simple single loop through the list of filenames insufficient for this task?

1. Read all filenames into an array and find the length of the longest one.
2. Column width = longest name + 2 spaces.
3. Number of columns = terminal width / column width (at least 1).
4. Number of rows = ceil(number of files / number of columns).
5. The items fill the **first column from top to bottom**, then the second column, and so on. So item `i` is in row `i % rows` and column `i / rows`.

The terminal can only print line by line (row by row), from left to right. Therefore we loop over **rows**, and for each row we print the items `r`, `r + rows`, `r + 2*rows`, ..., padding each one to the column width.

A single loop that prints `names[0], names[1], names[2] ...` in order would place consecutive items **next to each other on the same line** (across), not under each other (down). Also, the number of rows must be known before printing the first line, which requires knowing all the names and the longest name beforehand.

### Q4. What is the purpose of the `ioctl` system call in this context? What would be the limitations of your program if you only used a fixed-width fallback (e.g., 80 columns) instead of detecting the terminal size?

`ioctl()` is a general system call for sending special requests to a device driver. With the `TIOCGWINSZ` request, the terminal driver fills a `struct winsize` whose `ws_col` field tells us the **current width of the terminal in characters**:

```c
struct winsize ws;
ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);
int width = ws.ws_col;
```

If the program always assumed 80 columns:
- On a **wider** terminal, it would use only part of the screen and print more rows than needed.
- On a **narrower** terminal (less than 80 columns), the lines would be too long, wrap onto the next line, and the columns would become misaligned and hard to read.
- It would not adapt when the user resizes the window.

The fixed width (80) is still used as a fallback when `ioctl` fails, for example when the output is redirected to a file or a pipe, which has no width.

---

## Feature 4 — ls-v1.3.0: Horizontal Column Display (-x)

### Q5. Compare the implementation complexity of the "down then across" (vertical) printing logic versus the "across" (horizontal) printing logic. Which one requires more pre-calculation and why?

The **vertical** (down then across) logic requires more pre-calculation. Before printing anything it must know the total number of files, the longest name, the number of columns and the **number of rows**, because the index of each item on a line depends on the number of rows (`i = r + c * rows`). It loops over rows and columns and jumps around the array.

The **horizontal** (across) logic is simpler. It only needs the column width (from the longest name) and the terminal width. It walks through the array once, in order, keeping track of the current horizontal position, and prints a newline when the next item would go past the terminal width. It does not need to know the number of rows in advance.

### Q6. Describe the strategy you used in your code to manage the different display modes (-l, -x, and default). How did your program decide which function to call for printing?

I used one global integer flag, `display_mode`, with three named values:

```c
#define MODE_DEFAULT    0   /* down then across */
#define MODE_LONG       1   /* -l */
#define MODE_HORIZONTAL 2   /* -x */
```

In `main()`, the `getopt()` loop sets this flag: `-l` sets `MODE_LONG` and `-x` sets `MODE_HORIZONTAL` (if both are given, the last one wins, like the real `ls`). If no option is given, the flag keeps its default value.

In `do_ls()`, after all filenames have been read into the array (and sorted), an `if / else if / else` checks the flag and calls exactly one display function: `display_long()`, `display_horizontal()` or `display_vertical()`. Reading the directory is shared by all modes; only the printing step is different.

---

## Feature 5 — ls-v1.4.0: Alphabetical Sort

### Q7. Why is it necessary to read all directory entries into memory before you can sort them? What are the potential drawbacks of this approach for directories containing millions of files?

`readdir()` returns the entries one at a time, in the order they are stored in the directory (which is not alphabetical). To sort, we must compare every name with others, and the first name to print might be the last one returned by `readdir()`. Therefore all names must be collected in memory (in a dynamic array) before sorting and printing.

Drawbacks for very large directories:
- **Memory:** every name, plus a pointer for each one, must be stored at the same time. Millions of names can use hundreds of megabytes of RAM.
- **Delay:** nothing is printed until the whole directory has been read and sorted, so the user waits with no output.
- **Time:** sorting costs O(n log n) comparisons, and `realloc()` may copy the array several times as it grows.

### Q8. Explain the purpose and signature of the comparison function required by `qsort()`. How does it work, and why must it take `const void *` arguments?

`qsort()` is a general sorting function that can sort an array of **any** type. It does not know how to compare our elements, so we give it a comparison function (through a **function pointer**) that it calls whenever it needs to compare two elements:

```c
int compare_names(const void *a, const void *b)
{
    const char *s1 = *(const char **)a;
    const char *s2 = *(const char **)b;
    return strcmp(s1, s2);
}

qsort(names, n, sizeof(char *), compare_names);
```

- It must return a **negative** number if `a` should come before `b`, **zero** if they are equal, and a **positive** number if `a` should come after `b`. `strcmp()` already returns values in this form.
- `qsort()` passes **pointers to the elements** of the array. Our elements are `char *` (strings), so each argument is really a `char **`. We cast it to `const char **` and dereference it to get the string.
- The parameters are `void *` because `qsort()` is generic: `void *` can point to any type, so the same `qsort()` works for ints, structs or strings. They are `const` because the comparison function should only read the elements, never modify them.

---

## Feature 6 — ls-v1.5.0: Colorized Output

### Q9. How do ANSI escape codes work to produce color in a standard Linux terminal? Show the specific code sequence for printing text in green.

An ANSI escape code is a special sequence of characters that the terminal does not print; instead it interprets it as a command (for example, "change the text color"). It starts with the **escape character** (ASCII 27, written `\033` in C), followed by `[`, one or more numbers separated by `;`, and the letter `m`.

- `0` = reset/normal, `7` = reverse video
- `31` = red, `32` = green, `34` = blue, `35` = magenta/pink

Everything printed after the code appears in that color until the reset code `\033[0m` is printed.

Printing text in green:

```c
printf("\033[0;32m%s\033[0m", "text");
```

`\033[0;32m` switches to green, then the text is printed, then `\033[0m` resets the color so the following output is normal.

### Q10. To color an executable file, you need to check its permission bits. Explain which bits in the `st_mode` field you need to check to determine if a file is executable by the owner, group, or others.

The three execute bits are:

| Who | Macro | Octal |
|---|---|---|
| Owner | `S_IXUSR` | `0100` |
| Group | `S_IXGRP` | `0010` |
| Others | `S_IXOTH` | `0001` |

A file is treated as executable if it is a regular file and **any** of these bits is set:

```c
if (S_ISREG(mode) && (mode & (S_IXUSR | S_IXGRP | S_IXOTH)))
    /* executable -> green */
```

`S_IXUSR | S_IXGRP | S_IXOTH` combines the three masks into one (`0111`), and `&` checks whether at least one of those bits is on. The `S_ISREG` check is needed because directories also have `x` bits (meaning "can enter the directory"), but they should be colored blue, not green.

---

## Feature 7 — ls-v1.6.0: Recursive Listing (-R)

### Q11. In a recursive function, what is a "base case"? In the context of your recursive ls, what is the base case that stops the recursion from continuing forever?

A **base case** is the condition in which a recursive function does **not** call itself again, so the chain of calls stops and returns.

In my recursive `do_ls()`, the recursion stops when a directory has **no subdirectories to visit**: the loop finds no entry that is a directory, so no further `do_ls()` call is made and the function returns. Because every directory tree has a finite depth, every path eventually reaches such a directory.

Two safeguards make sure this always happens:
- The entries `.` (current directory) and `..` (parent directory) are skipped; otherwise `do_ls()` would call itself on the same or parent directory forever.
- `lstat()` is used instead of `stat()`, so a symbolic link pointing to a directory (for example, to a parent directory) is not followed, which could otherwise create an infinite loop.

### Q12. Explain why it is essential to construct a full path (e.g., "parent_dir/subdir") before making a recursive call. What would happen if you simply called `do_ls("subdir")` from within the `do_ls("parent_dir")` function call?

`readdir()` returns only the **name** of an entry (for example `subdir`), not its location. A name without a path is a **relative path**, and the operating system resolves relative paths from the program's **current working directory**, not from the directory being listed.

My program never changes its current directory, so if `do_ls("parent_dir")` called `do_ls("subdir")`, `opendir("subdir")` would look for `./subdir` (in the folder where `ls` was started) instead of `parent_dir/subdir`. It would either fail with "Cannot open directory", or worse, list a different folder that happens to have the same name. The same problem would occur for `lstat()` calls on the entries.

By building the full path (`parent_dir/subdir`, then `parent_dir/subdir/deeper`, and so on), every call knows the exact location of its directory, no matter how deep the recursion goes. The full path is also printed as the header (`parent_dir/subdir:`), which is the same format as the real `ls -R`.
