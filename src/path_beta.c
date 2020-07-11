#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <Windows.h>
#include <math.h>
#include <time.h>
#include <conio.h>
#include <stdarg.h>


/*********************宏定义*********************/
#define STACK_VOLUME 11                         //路径栈大小的最大值，由于每个城市最多只经过1次，最大值为10，此处设为11
#define RISK_OF_BUS 2                           //巴士单位时间风险值
#define RISK_OF_TRAIN 5							//火车单位时间风险值
#define RISK_OF_AIR 9							//飞机单位时间风险值
#define TIME_PUSH_INTERVAL 3					//系统时间推进间隔
#define DIR_OF_LOG "CLI_LOG.txt"				//日志文件路径
#define DIR_OF_TIMETABLE "TimeTable.csv"		//时刻表数据文件路径

/*********************全局变量*******************/
char Cur_User[50] = { "root" };					//当前用户名
int User_Num = 0;								//用户总量
int Sys_Time = 12;								//当前系统时间，默认从12点开始
const float RISK_OF_CITIES[10] = { 0.2, 0.5, 0.5, 0.2, 0.9, 0.9, 0.2, 0.9, 0.5, 0.2 };//每一城市的单位时间风险值
const char City_List[10][30] = { "HONGKONG\0","BEIJING\0","SHANGHAI\0","GUANGZHOU\0","SHENZHEN\0",
										 "CHENGDU\0","XIAMEN\0","WUHAN\0","CHANGSHA\0","LASA\0" };//城市名列表

//部分函数声明
int cmd(char* cmd, char* result);//cmd pipe
int Mprintf(const char *format, ...);//重写了printf，使得可以同时向文件流和标准输出流进行输出

/**********************结构体********************/

//图的边界点，即班次，除了type，都以纯数字方式存储
typedef struct TBnode
{
	int dp_time;			//出发时间
	char type;				//表示交通工具：1-飞机，2-火车，3-巴士
	char from;				//出发城市ID
	char to;				//到达城市ID
	int dur;				//该班次从出发到到达所需时间
	struct TBnode* Next;	//邻接表中该节点的next节点
}Time_Node, *TBnode;

//图的顶点节点，即城市
typedef struct city
{
	char num;
	TBnode First_Node;
}city;

city Cities[10] = { 0 };	//图的顶点节点以顺序表方式存储

//路径栈，用以存储路径
typedef struct Node_Stack
{
	float risk;				//该条路径的风险值
	int time;				//该条路径花费的总时间
	TBnode* bot;			//栈底元素，即路径的起点
	TBnode* top;			//栈顶元素+1
	//char Cur_Num;
}Node_Stack, *Stack_P;

Stack_P Path[100000] = { 0 }; //路径栈，设最多有100000条路径

//用户
typedef struct User
{
	int state;				//0=未出发，1 = 旅行中，2=已完成
	char Name[50];			//用户名
	Stack_P Cur_Path;		//当前用户所设置的旅行路径
	TBnode* Cur_Node;		//当前用户正在/即将搭乘的班次
}Struct_of_User, *User;

Struct_of_User User_List[100];//设最多有100个用户，用户信息的指针以顺序表的结构存储，方便查询
User Cur_UserP;				//当前用户（指针）

//初始化栈操作，为路径栈分配空间，返回栈指针
Stack_P Init_Stack()
{
	Stack_P New_Stack = (Stack_P)malloc(sizeof(Node_Stack));
	TBnode *p = (TBnode*)calloc(STACK_VOLUME, sizeof(TBnode));
	New_Stack->bot = p;
	New_Stack->top = p;
	return New_Stack;
}

//栈的判空操作
bool Stack_Is_Empty(Stack_P S)
{
	if (S->bot == S->top)
		return true;
	else
		return false;
}

//栈的压栈操作
bool Push_Node(Stack_P S, TBnode Node)
{
	*(S->top) = Node;
	//S->Cur_Num ++;
	S->top++;
	return true;
}

//栈的出栈操作
TBnode Pop_Node(Stack_P S)
{
	if (!Stack_Is_Empty(S))
	{
		TBnode node = *(S->top - 1);
		S->top--;
		return node;
	}
	else
		return NULL;
}

//取得栈顶元素
TBnode Get_Top(Stack_P S)
{
	if (!Stack_Is_Empty(S))
		return *(S->top - 1);
	else
		return NULL;
}

//取得栈底元素
TBnode Get_Bot(Stack_P S)
{
	if (!Stack_Is_Empty(S))
		return *(S->bot);
	else
		return NULL;
}

//销毁栈
void Del_Stack(Stack_P S)
{
	free(S->bot);
	free(S);
}

//创建图的边界点，即班次
TBnode Create_Node(int dp_time, char type, char from, char to, int dur, TBnode Next)
{
	TBnode New = (TBnode)malloc(sizeof(Time_Node));
	New->dp_time = dp_time;
	New->type = type;
	New->from = from;
	New->to = to;
	New->dur = dur;
	New->Next = Next;
	return New;
}

//把边界点插入到图，即邻接表中
bool Insert_Node(int City, TBnode node)
{
	TBnode temp = Cities[City].First_Node;
	if (temp)
	{
		while (temp->Next)
			temp = temp->Next;
		temp->Next = node;
		return 0;
	}
	else
		Cities[City].First_Node = node;
	return 1;
}

//打印时刻表信息
void printTB()
{
	for (int i = 0; i <= 9; i++)
	{
		TBnode temp = Cities[i].First_Node;
		while (temp)
		{
			Mprintf("From:%c To:%c Type:%c Departure:%d Duration:%d\n",
				temp->from + '0', temp->to + '0', temp->type, temp->dp_time, temp->dur);
			temp = temp->Next;
		}
	}
}

//导入时刻表
void Load_Table()
{
	FILE *fp;
	char str[1000] = { 0 };
	fp = fopen(DIR_OF_TIMETABLE, "r");
	fgets(str, 1000, fp);
	fgets(str, 1000, fp);

	int Cur_City = 0;
	while (fgets(str, 1000, fp))
	{
		int i = 0;
		int len = strlen(str);
		int time = 0;
		for (; i <= len - 1; i++)
		{
			switch (str[i])
			{
			case'0':
			case'1':
			case'2':
			case'3':
			case'4':
			case'5':
			case'6':
			case'7':
			case'8':
			case'9':
				Insert_Node(Cur_City, Create_Node(time, str[i + 1], Cur_City, str[i] - '0', str[i + 2] - '0', NULL));
				i += 2;
				break;
			case';':
				break;
			case',':
				time += 1;
				break;
			}
		}
		Cur_City++;
	}
	printTB();
	fclose(fp);
}

//计算两个时刻之间的差值
int Time_Dif(int early, int late)
{
	int Time_Dif = 0;
	if (early > late)
		Time_Dif = 24 - (early - late);
	else
		Time_Dif = late - early;
	return Time_Dif;
}

//返回该交通工具对应单位时间风险
int Travel_Risk(char type)
{
	if ('1' == type)
		return RISK_OF_AIR;
	else if ('2' == type)
		return RISK_OF_TRAIN;
	else if ('3' == type)
		return RISK_OF_BUS;
	return 0;
}

//记录下当前的路径，一般在DFS中搜索到能到达目的地的边界点时调用
void Record_Path(Stack_P Src, int Path_Num)
{
	Path[Path_Num] = Init_Stack();
	TBnode *temp = Src->bot;
	int Last_Arv_Time = Sys_Time;
	int Next_Dp_Time = (*temp)->dp_time;
	int Total_Time = 0;
	float Total_Risk = 0;

	while (temp != Src->top)
	{
		Next_Dp_Time = (*temp)->dp_time;
		Total_Time += Time_Dif(Last_Arv_Time, Next_Dp_Time) + (*temp)->dur;
		Total_Risk += Time_Dif(Last_Arv_Time, Next_Dp_Time) * RISK_OF_CITIES[(*temp)->from] + RISK_OF_CITIES[(*temp)->from]*(*temp)->dur * Travel_Risk((*temp)->type);
		Last_Arv_Time = Next_Dp_Time + (*temp)->dur;
		Push_Node(Path[Path_Num], *temp);
		temp++;
	}
	Path[Path_Num]->time = Total_Time;
	Path[Path_Num]->risk = Total_Risk;
}

//对班次构成的邻接表进行深度优先搜索，以算出从某个城市出发到达另一城市，所有可能的路径，该路径由不同班次构成，而非城市
void DFS(Stack_P Cur_Path, TBnode Cur_Node, int* Path_Num, char to, bool* flag)
{
	if (Cur_Node == NULL);
	else if (Cur_Node->to == to)
	{
		Push_Node(Cur_Path, Cur_Node);
		Record_Path(Cur_Path, *Path_Num);
		*Path_Num += 1;
		Pop_Node(Cur_Path);
		DFS(Cur_Path, Cur_Node->Next, Path_Num, to, flag);//向右搜索
	}
	else
	{
		if (flag[Cur_Node->to])
			DFS(Cur_Path, Cur_Node->Next, Path_Num, to, flag);//向右搜索
		else
		{
			Push_Node(Cur_Path, Cur_Node);
			flag[Cur_Node->from] = true;
			DFS(Cur_Path, Cities[Cur_Node->to].First_Node, Path_Num, to, flag);//向下搜索
			Pop_Node(Cur_Path);
			flag[Cur_Node->from] = false;
			DFS(Cur_Path, Cur_Node->Next, Path_Num, to, flag);//向右搜索
		}
	}
}

//交换路径表中的两个路径，一般是在对搜索到的所有路径根据风险和时间进行排序时调用
void Swap_Path(int i, int j)
{
	Stack_P temp;
	temp = Path[i];
	Path[i] = Path[j];
	Path[j] = temp;
}

//改进选择排序，将符合条件若干个最优路径拍到路径表的前面，返回满足条件的最优路径个数
int Sort_Path(int Path_Num, int Time_Limit)
{
	if(Time_Limit)//限时风险最小策略
    {
        int i = 0;
        int Best_i = 0x0FFFFFFF;
        int cnt = 0;  
        for(;i < Path_Num; i++)
        {
            if(Path[i]->time <= Time_Limit)
                {
                    Best_i = i;
                    break;
                }
        }
        if(Best_i == 0x0FFFFFFF) 
            return 0;
        Swap_Path(Best_i, 0);
        bool found = 1; 
        int ini = Best_i + 1;
        Best_i = 0;
        while(found == 1)
        {
            found = 0;
            for (int j = ini; j < Path_Num; j++)
            {
                if (Path[j]->time <= Time_Limit && Path[j]->risk < Path[Best_i]->risk)
                {
                    Best_i = j;
                    found = 1;
                }
                else if (Path[j]->time <= Time_Limit && Path[j]->risk == Path[Best_i]->risk)
                {
                    if (Path[j]->time <= Path[Best_i]->time)
                    {
                        Best_i = j;
                        found = 1;
                    }
                }
            }
            if(found)
            {
                Swap_Path(Best_i,cnt);
                Best_i = cnt;
                cnt++;
            }
            else
            {
                if(cnt==0)
                    return 1;
                else
                    return cnt;
            }
        }
    }
    else//不限时风险最小策略
    {
        int i = 0;
        int Best_i;
        float Best_risk = Path[0]->risk;
        for (; i < Path_Num - 1; i++)
        {
            Best_i = i;
            int j = i + 1;
            for (; j < Path_Num; j++)
            {
                if (Path[j]->risk < Path[Best_i]->risk)
                {
                    Best_i = j;
                }
                else if (Path[j]->risk == Path[Best_i]->risk)
                {
                    if (Path[j]->time < Path[Best_i]->time)
                    {
                        Best_i = j;
                    }
                }
            }
            if(Path[Best_i]->risk > Best_risk)
                return i;
            Best_risk = Path[Best_i]->risk;
            Swap_Path(i,Best_i);
        }
        return Path_Num;
    }
}

//取得城市名
char* Get_City(int num)
{
    return City_List[num];
}

//将某一有班次构成的路径完整地打印出来
void Print_Path(int Path_Num, int Time_Limit)
{
	int i = 0;
	int j = 0;//输出的第j条路径
	bool best_flag = 1;
	float Best_risk;

	for (; i < Path_Num; i++)
	{
		if (Time_Limit == 0 || Path[i]->time <= Time_Limit)
		{
			if (best_flag)
			{
				Cur_UserP->Cur_Path = Path[i];
				Cur_UserP->Cur_Node = Path[i]->bot;
				Cur_UserP->state = 0;
				best_flag = false;
				Best_risk = Path[i]->risk;
			}
			if(Path[i]->risk == Best_risk)
			{
				j++;
				Mprintf("\nPath%d: Risk:%.1f Time:%d hrs\n", j, Path[i]->risk, Path[i]->time);
				TBnode *temp = Path[i]->bot;
				int Last_Arv_Time = Sys_Time;
				int Next_Dp_Time = (*temp)->dp_time;
				while (temp != Path[i]->top)
				{
					Next_Dp_Time = (*temp)->dp_time;
					if (Last_Arv_Time != Next_Dp_Time)
						Mprintf("%2d:00-%2d:00: Wait for %d hours;\n",
							Last_Arv_Time, Next_Dp_Time, Time_Dif(Last_Arv_Time, Next_Dp_Time));

					Mprintf("%2d:00-%2d:00: Take a ", (*temp)->dp_time, ((*temp)->dur + (*temp)->dp_time) % 24);
					if ((*temp)->type == '1') Mprintf("flight ");
					else if ((*temp)->type == '2') Mprintf("train ");
					else if ((*temp)->type == '3') Mprintf("bus ");
					Mprintf("from %s to %s;\n",
						Get_City((*temp)->from), Get_City((*temp)->to));
					Last_Arv_Time = (Next_Dp_Time + (*temp)->dur)%24;
					temp++;
				}
			}
			else
				break;
		}
	}
	Mprintf("\n");
}

//路径计算与输出模块
void Calc_Path()
{
	bool flag[10] = { 0 };
	char from, to;
    char c = getchar();
    if (c == '\n')
    {
	    Mprintf("Please enter your starting point and destination, in the format of:[City ID] [City ID].\n");
	    system("type citylist.txt");
        Mprintf("\n");
    }
	scanf("%c %c", &from, &to);
	fflush(stdin);
	Mprintf("When would you like to start your trip? (-1 = default, 0 = now, 1 = 1 hour later...)\n");
	int Actual_System_Time = Sys_Time;
	int temp;
	scanf("%d", &temp);
    if(temp == -1)
        temp = 24 -Sys_Time;
	Sys_Time = (Sys_Time + temp) % 24;
	fflush(stdin);
	Stack_P Cur_Path = Init_Stack();
	int p_cnt = 0; //路径计数
	int *Path_Num = &p_cnt;
	DFS(Cur_Path, Cities[from - '0'].First_Node, Path_Num, to - '0', flag);
	Mprintf("sorting...\n");
	Mprintf("What's your time limit ? (unit:hour, 0 == No Limit)\n");
	int Time_Limit = 0;
	scanf("%d", &Time_Limit);
    Sort_Path(*Path_Num, Time_Limit);
	Print_Path(*Path_Num, Time_Limit);
	Sys_Time = Actual_System_Time;
    Mprintf("\rtime is: %d:00", Sys_Time);
}

//根据当前用户名，取得该用户名对应的用户结构体的指针
User Get_User()
{
	User temp = User_List;
	for (int i = 0; i < User_Num; i++, temp++)
	{
		if (strcmp(Cur_User, temp->Name) == 0)
			return temp;
	}
	strcpy(temp->Name, Cur_User);
	Mprintf("Created User: %s\n", Cur_User);
	User_Num++;
	return temp;
}

//切换用户
void Switch_User()
{
	char c = getchar();
    if(c == '\n')
        Mprintf("Please input your name:");
	scanf("%s", Cur_User);
	//Mprintf("\r                                  ");
	fflush(stdin);
	//system("cls");
	Cur_UserP = Get_User();

}

//查询某一用户的旅行信息
bool Query_User()
{
	char temps[50];
    char c = getchar();
    if(c == '\n')
	    Mprintf("Please enter a name:");
	scanf("%s", temps);
	fflush(stdin);
	for (int i = 0; i < User_Num; i++)
	{
		if (strcmp(User_List[i].Name, temps) == 0)
		{
			Mprintf("\nState:%d (0-waiting 1-traveling 2-finished)\n", User_List[i].state);
			if (User_List[i].state != 2)
			{
				TBnode *temp = User_List[i].Cur_Node;
				if (temp == NULL)continue;
				Mprintf("%2d:00-%2d:00: Take a ", (*temp)->dp_time, ((*temp)->dur + (*temp)->dp_time) % 24);
				if ((*temp)->type == '1') Mprintf("flight ");
				else if ((*temp)->type == '2') Mprintf("train ");
				else if ((*temp)->type == '3') Mprintf("bus ");
				Mprintf("from %s to %s;\n", Get_City((*temp)->from), Get_City((*temp)->to));
			}
			return true;
		}
	}
	return false;
}

//每到整点调用，更新所有用户的旅行信息
void Update_User()
{
	int i = 0;
	for (; i < User_Num; i++)
	{
		if (User_List[i].Cur_Node == NULL) continue;
		if (User_List[i].Cur_Node != User_List[i].Cur_Path->top)
		{
			if ((*User_List[i].Cur_Node)->dp_time == Sys_Time)
			{
				User_List[i].state = 1;
			}
			else if (((*User_List[i].Cur_Node)->dp_time + (*User_List[i].Cur_Node)->dur) % 24 == Sys_Time && User_List[i].state == 1)
			{
				User_List[i].state = 0;
				User_List[i].Cur_Node++;
				if (User_List[i].Cur_Node == User_List[i].Cur_Path->top)
					User_List[i].state = 2;
			}
		}
	}
}

//类shell模块，作为主模块的中断程序存在，监测到键盘中断":"时进入，负责解析用户命令，调用相应子模块
void Command_Input()
{
	char comd[100] = { 0 };
	scanf("%s", comd);
	//getchar();
	if (strcmp(comd, "getpath") == 0)
		Calc_Path();
	else if (strcmp(comd, "su") == 0)
    {
		Switch_User();
        Mprintf("%s:# ",Cur_User);
        Command_Input();
    }
	else if (strcmp(comd, "query") == 0)
	{
		if (!Query_User())
		{
			Mprintf("\nNo relevant path found!\n");
            Mprintf("\rtime is: %d:00", Sys_Time);
		}
	}
    else if (strcmp(comd, "time") == 0)
    {
        Mprintf("Current System Time is: %d:00\n%s:# ",Sys_Time, Cur_User);
        Command_Input();
    }
    else if (strcmp(comd, "continue") == 0)
    {
        Mprintf("\rtime is: %d:00", Sys_Time);
    }
    else if (strcmp(comd, "cls") == 0)
    {
        system("cls");
        Mprintf("%s:# ", Cur_User);
        Command_Input();
    }
    else if (strcmp(comd, "help") == 0)
    {
        system("type commands.txt");
        Mprintf("\n\n%s:# ", Cur_User);
        Command_Input();
    }
	else
        {
		    Mprintf("No such command as \"%s\". Try to enter \"help\" to get more tips.\n%s:# ", comd, Cur_User);
            Command_Input();
        }
	fflush(stdin);
}

//主模块，负责时间推进
int main()
{
	Load_Table();
	Cur_UserP = Get_User();
	system("cls");
	Mprintf("%s:# ", Cur_User);
	Command_Input();
	time_t Last_Time = time(NULL);
	time_t Cur_Time = Last_Time;
	Mprintf("\r                              ");
	Mprintf("\rtime is: %d:00 ", Sys_Time);
	Update_User();
	while (1)
	{
		if (kbhit())//检测键盘中断
		{
			char command = getch();
			if (command == ':')//检测到键盘中断“:”，调用类shell模块进入命令行模式
			{
				Mprintf("\r               ");
				Mprintf("\r%s:# ", Cur_User);
				Command_Input();
				Last_Time = time(NULL);
			}
		}
		Cur_Time = time(NULL);
		if (Cur_Time - Last_Time == TIME_PUSH_INTERVAL)
		{
			Sys_Time = (Sys_Time + 1) % 24;
			Mprintf("\rtime is: %d:00 ", Sys_Time);
			Last_Time = Cur_Time;
			Update_User();//每个整点调用Update_User函数更新用户旅行信息
		}
	}
}

/*
int cmd(char* cmd, char* result)
{
	char buffer[10240];
	FILE* pipe = popen(cmd, "r");
	if (!pipe)
		return -1;
	while (!feof(pipe))
	{
		if (fgets(buffer, 4096, pipe))
		{
			strcat(result, buffer);
		}
	}
	pclose(pipe);
	return 0;
}*/

//
int Mprintf(const char *format, ...)//按照printf函数原型重写了printf，使得标准输出流和文件流能同时使用
{
	FILE *log = fopen(DIR_OF_LOG, "a+");
	va_list arg;
	int done;
	va_start(arg, format);
	time_t time_seconds = time(0);
	struct tm* now_time = localtime(&time_seconds);
	fprintf(log, "\n[%d-%d-%d %d:%d:%d]", now_time->tm_year + 1900, now_time->tm_mon + 1,
		now_time->tm_mday, now_time->tm_hour, now_time->tm_min,
		now_time->tm_sec);

	vprintf(format, arg);
	done = vfprintf(log, format, arg);
	va_end(arg);
	return 1;
}
