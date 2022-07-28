// #ifdef COMPILETIME
// #include <stdio.h>
// #include <malloc.h>

// //定义malloc 包装函数
// void *mymalloc(size_t size)
// {
//   void *ptr = malloc(size);
//   printf("=================malloc(%d) = %p\n", (int)size, ptr);
//   return ptr;
// }

// //定义free 包装函数
// void *myfree(void *ptr)
// {
//   free(ptr);
//   printf("=================free(%p) = %p\n",  ptr);
// }

// #endif