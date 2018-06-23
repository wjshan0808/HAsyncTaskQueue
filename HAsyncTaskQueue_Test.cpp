#include "HAsyncTaskQueue.h"

#include <iostream>


using namespace std;


class CTPMTest
{
public:
  int Test()
  {
    std::cout << "Test" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(1));
    return 0;
  };
};
class CTPMTest2
{
public:
  void Test2(const std::string& str, int i)
  {
    std::cout << "Test2: " << str.c_str() << ", i: " << i << std::endl;
  }
};
class CTPMTest3
{
public:
  void operator()(const std::string& str, int i, int j)
  {
    std::cout << "Test3: " << str.c_str() << ", i+j: " << i + j << std::endl;
  }
};


int Class_Test()
{
  std::shared_ptr<CHAsyncTaskQueue> m_pTPM = std::make_shared<CHAsyncTaskQueue>();

  CTPMTest t;
  m_pTPM->Enqueue(std::mem_fn(&CTPMTest::Test), &t);

  CTPMTest2 t2;
  m_pTPM->Enqueue(&CTPMTest2::Test2, &t2, "test22", 20);
  m_pTPM->Enqueue(std::bind(&CTPMTest2::Test2, &t2, "test2", 10));

  //be careful , not recommended !!!
  CTPMTest3 t3;
  m_pTPM->Enqueue(t3, "test3", 10, 20);

  return 0;
}

void Lambda_Test()
{
  std::unique_ptr<CHAsyncTaskQueue> pool = std::unique_ptr<CHAsyncTaskQueue>(new CHAsyncTaskQueue());

  std::vector< std::future<int> > results;

  for (int i = 0, j = 0; i < 16; ++i, j += i)
  {
    results.emplace_back(pool->Enqueue([i, j]
    {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      return i + j;
    }));
  }

  for (auto && result : results)
    std::cout << result.get() << ' ';
  std::cout << std::endl;


  std::future<std::string> f = pool->Enqueue([]()->std::string { std::cout << "Lambda_Test " << std::endl;  return "hell0 world.";  });
  std::cout << f.get().c_str() << std::endl;

}

void Global_Test(int a)
{
  std::cout << "Global_Test " << a << ", Current thread id: " << std::this_thread::get_id() << std::endl;
}

//int ThreadPool_main()
int main()
{
  Class_Test();

  Lambda_Test();

  CHAsyncTaskQueue pool;
  pool.Enqueue(Global_Test, 1);

  getchar();

  return 0;
}
