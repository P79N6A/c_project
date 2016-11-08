#include <iostream>
using namespace std;

// 函数声明
/**
*如果允许不声明，那编译时就得扫描两遍，第一个用来确定函数原型，这样的话，编译时间就浪费很多
*
*/
int max(int, int );

int main ()
{
		// 局部变量声明
		int a = 100;
		int b = 200;
		int ret;

		// 调用函数来获取最大值
		ret = max(a, b);

		cout << "Max value is : " << ret << endl;

		return 0;
}

// 函数返回两个数中较大的那个数
int max(int num1, int num2) 
{
		// 局部变量声明
		int result;

		if (num1 > num2)
				result = num1;
		else
				result = num2;

		return result; 
}
