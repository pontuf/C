#include <math.h>
#include <stdio.h>
#include <locale.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <dirent.h>
#include <time.h>

//функция выбора для сортировки (в нашем случае выбирает всё)
static int one(const struct dirent *unused){
    return 1;
}

//функция проверки правильности пути
//если неверный путь или не хватает прав, то выход с ошибкой
char checkpath(char *arg){
    DIR *dp;
    dp = opendir(arg);
    if (dp == NULL){
        puts("ERROR! At the end of the command, the correct path to the directory was expected.\
        \nThere may not be permissions to this path.");
        return 1;
    }
    return 0;
}

//функция вывода простого списка, если не было -l параметра
char simplelist(char *arg, char rev){
    if (checkpath(arg))
        return 1; 
    
    struct dirent **eps;
    int n = 0;
    
    n = scandir (arg, &eps, one, alphasort);
    if (n >= 0){
        int i = 0;
        for (int cnt = 0; cnt < n; ++cnt){
            i = cnt;
            if (rev)
                i = n - cnt - 1; // обратный порядок
            if (eps[i]->d_name[0] != '.') 
                printf("%-s    ", eps[i]->d_name);
        }
        if (n > 2)
            putchar('\n');
    }
    return 0;
}

//функция формирования символьного представления прав
//k -- указывает на тип прав, 4 -- владелец, 2 -- группа, 1 -- остальные
char *getrights(char mask, char spec, char k){
    static char res[4];
    res[3] = '\0';
    
    if (mask & 4)
        res[0] = 'r';
    else 
        res[0] = '-';
    if (mask & 2)
        res[1] = 'w';
    else 
        res[1] = '-';
    if (mask & 1)
        res[2] = 'x';
    else 
        res[2] = '-';
    
    // проверяем sticky бит и другие спец. биты
    // с помощью k
    switch (k & spec){
        case 4:
        case 2:
            res[2] = 's';
            break;
        case 1:
            res[2] = 't';
            break;
    }
    return res;
}

//функция формирования полного пути из директории и названия файла
char *fullpath(const char *dir, const char *name){
    static char res[80];
    
    strcpy(res, dir);
        
    if (res[strlen(res) - 1] != '/')
        strcat(res, "/");
    strcat(res, name);
    
    return res;
}

// функция перевода размеров файлов
// сама строка вывода в res, возвращается её длина
char hsize(long long num, char *res){
    double frac = 0.0;
    char len = 1;
    char temp[8];
    
    if (num / 1099511627776){
        frac = (double) num / 1099511627776;
        len = sprintf(temp,"%.1fT", frac);
    }
    else if (num / 1073741824){
        frac = (double) num / 1073741824;
        len = sprintf(temp,"%.1fG", frac);
    }
    else if (num / 1048576){
        frac = (double) num / 1048576;
        len = sprintf(temp,"%.1fM", frac);
    }
    else if (num / 1024){
        frac = (double) num / 1024;
        len = sprintf(temp,"%.1fk", frac);
    }
    else 
        len = sprintf(temp,"%lld", num);
    
    strcpy(res, temp);
    return len;
}

// функция сдвига для красивых границ столбцов
void shift(char max, char cur){
    for (int i = 0; i < (max - cur); ++i)
        putchar(' ');
}

int main(int argc, char *argv[]){   
    // автоматический подбор локали
    setlocale(LC_ALL, "");
    
    // путь из аргументов командной строки (или текущая директория)
    char *path; 
    // флаги аргументов командной строки 
    char reverse = 0; // -r
    char hread = 0; // -h
    char table = 0; // -l
    
    // нет аргументов
    if (argc == 1){
        simplelist("./", reverse); // по текущей директории
        return 0;
    }
    //только путь
    else if (argc == 2 && argv[argc-1][0] != '-'){
        return simplelist(argv[argc-1], reverse);
    } 
    //путь и аргументы
    else if (argc >= 3 && argv[argc-1][0] != '-'){
        if (checkpath(argv[argc-1]))
            return 1;
        else{
            --argc; 
            path = argv[argc];
        }
    } 
    // только аргументы
    else{
        if (checkpath("./"))
            return 1;
        else
            path = "./"; // по текущей директории
    }
    
    // цикл взятия аргументов
    char arg = 0; 
	opterr=0;
    while ((arg = getopt(argc,argv,"rlh")) != -1){
    	
		switch (arg){
            case 'l': table = 1; break;
            case 'r': reverse = 1; break;
            case 'h': hread = 1; break;
            case '?': 
                printf("Wrong argument: -%c\n",optopt); 
                return 1;
        }
	}
	
	//если не режим -l, то просто вывод списка
	//при том -r работает, а -h нет
	if (!table){
        simplelist(path, reverse);
        return 0;
    }
    
    //РЕЖИМ -l
    struct dirent **eps;
    int n = 0;
    // получаем сортированный список файлов
    n = scandir (path, &eps, one, alphasort); 
    
    if (n < 0){
        puts("No files\n"); //
        return 1;
    }
    
    //цикл счета ширины столбцов
    struct stat temp;
    long long total = 0;
    int len = 1;
    char linkcol = 1;
    char uscol = 1;
    char grcol = 1;
    char sizecol = 1;
    char buf[8]; 
    char dcol = 1;
    char timecol = 4;
    short year = 0; // год изменения файла
    time_t rawtime;
    time(&rawtime);                               
    short cyear = localtime(&rawtime)->tm_year; // текущий год
    for (int i = 0; i < n; ++i){
        if (eps[i]->d_name[0] == '.')
            continue;
        
        if (lstat(fullpath(path, eps[i]->d_name), &temp) == -1){
            continue;
        }
        
        //расчет total blocks
        total += temp.st_blocks / 2; // блоки по 512 Кб
        
        if (temp.st_nlink > 1000)
            linkcol = 3;
        else if (temp.st_nlink > 99 && temp.st_nlink < 1000)
            linkcol = 3;
        else if (temp.st_nlink == 1000 || (temp.st_nlink > 9 && temp.st_nlink <= 99)){
            if (linkcol < 2)
                linkcol = 2;
        }
        
        len = strlen(getpwuid(temp.st_uid)->pw_name);
        if (len > uscol)
            uscol = len;
        
        len = strlen(getgrgid(temp.st_gid)->gr_name);
        if (len > grcol)
            grcol = len;
        
        if (hread){
            len = hsize(temp.st_size, buf);
            if (len > sizecol)
                sizecol = len;
        }
        else {
            if (temp.st_size <= 0)
                len = 0;
            else
                len = (int) floor(log10((double) temp.st_size));
            if (len > sizecol)
                sizecol = len;
        }
        
        if (localtime(&temp.st_mtime)->tm_mday > 9 && dcol == 1)
            dcol = 2;
        
        year = localtime(&temp.st_mtime)->tm_year;
        if ((cyear - year) < 1)
            timecol = 5;
    }
    
    //вывод total blocks перед самой таблицей
    char totalstr[8];
    if (hread){
        //функция обычно работает с байтами размеров файлов, но блоки в Килобайтах,
        //поэтому домножим
        hsize(total * 1024, totalstr); 
        printf("total %s\n", totalstr);
    }
    else
        printf("total %lld\n", (long long) total);
    
    //ОСНОВНОЙ ЦИКЛ ВЫВОДА ТАБЛИЦЫ
    struct stat sb;
    int cnt = 0;
    struct tm *time;
    for (int j = 0; j < n; ++j){
        cnt = j;
        if (reverse)
            cnt = n - j - 1;
        
        if (eps[cnt]->d_name[0] == '.')
            continue;
        
        if (lstat(fullpath(path, eps[cnt]->d_name), &sb) == -1){
            printf("? ");
            puts(eps[cnt]->d_name);
            continue;
        }
        
        //тип файла
        switch (sb.st_mode & S_IFMT) {
            case S_IFBLK:  putchar('b'); break;
            case S_IFCHR:  putchar('c'); break;
            case S_IFDIR:  putchar('d'); break;
            case S_IFIFO:  putchar('p'); break;
            case S_IFLNK:  putchar('l'); break;
            case S_IFREG:  putchar('-'); break;
            case S_IFSOCK: putchar('s'); break;
            default:       putchar('?'); break;
        }
        
        int access = sb.st_mode % 512;
        int spec = (sb.st_mode / 512) % 8;
        char userbits = access / 64;
        char groupbits = (access / 8) % 8;
        char otherbits = access % 8;
        
        printf("%s",getrights(userbits,spec,4));
        printf("%s",getrights(groupbits,spec,2));
        printf("%s ",getrights(otherbits,spec,1));
        
        if (sb.st_nlink < 1000){
            if (linkcol == 3)
                printf("%3d ", (long) sb.st_nlink);
            else if (linkcol == 2)
                printf("%2d ", (long) sb.st_nlink);
            else if (linkcol == 1)
                printf("%d ", (long) sb.st_nlink);
        }
        else if (sb.st_nlink == 1000){
        	shift(linkcol, 2);
        	printf("1K ");	
        } 
        else
            printf(">1K ");
        
        char *user = getpwuid(sb.st_uid)->pw_name;
        shift(uscol, strlen(user));
        printf("%s ", user);
        
        char *group = getgrgid(sb.st_gid)->gr_name;
        shift(grcol, strlen(group));
        printf("%s ", group);
        
        char size;
        if (hread){
            shift(sizecol, hsize(sb.st_size, buf));
            printf("%s ", buf);
        }
        else {
            if (sb.st_size <= 0)
                size = 0;
            else
                size = (int) floor(log10((double) sb.st_size));
            shift(sizecol, size);
            printf("%lld ", (long long) sb.st_size);
        }
        
        time = localtime(&sb.st_mtime);
        
        strftime(buf, 7, "%b", time); 
        printf("%s ", buf);
        
        size = floor(log10((double) time->tm_mday)) + 1;
        shift(dcol, size);
        printf("%d ", time->tm_mday);
        
        year = time->tm_year;
        
        if ((cyear - year) < 1){
            printf("%02d:%02d ", time->tm_hour, time->tm_min);
        }
        else {
            shift(timecol, 4);
            printf("%d ", year + 1900);
        }
        
        puts(eps[cnt]->d_name);
    }
	
	return 0;
}
