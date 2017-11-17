#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include <math.h>
#include <llvm-c/ExecutionEngine.h>
#include <llvm-c/Object.h>
#include <llvm-c/Core.h>
#include <llvm-c/BitReader.h>
#include <llvm-c/BitWriter.h>

size_t calculate_size(size_t size) {
    size_t page_size = getpagesize();
    return size < page_size ? page_size : ceil(size / page_size) * page_size;
}

uint8_t* code_section_cb (void *opaque, uintptr_t size,
      unsigned align, unsigned section_id, const char *section_name) {
   printf("Code %s\n", section_name);
   uint8_t *start = (uint8_t *)mmap(NULL,  calculate_size(size),
                                     PROT_WRITE | PROT_READ | PROT_EXEC,
                                     MAP_ANON | MAP_PRIVATE, -1, 0);
   if (start == (uint8_t*)-1) {
      //error
   }
   return start;
}

uint8_t* data_section_cb(void *opaque, uintptr_t size, unsigned align,
      unsigned section_id, const char *section_name, LLVMBool read_only) {
   printf("Data %s\n", section_name);
   return code_section_cb(opaque, size, align, section_id, section_name);
}

void destroy_cb(void *opaque) {

}

LLVMBool finalize_cb(void *opaque, char **error) {
   printf("Finalizing\n");
   return 0;
}

LLVMModuleRef create_module(char *filename) {
   LLVMMemoryBufferRef buf;
   char *error = NULL;
   if (LLVMCreateMemoryBufferWithContentsOfFile(filename, &buf, &error)) {
      printf("%s\n", error);
      return NULL;
   }

   LLVMModuleRef mod = LLVMModuleCreateWithName("test");
   if (LLVMParseBitcode(buf, &mod, &error)) {
      printf("%s\n", error);
      return NULL;
   }
   return mod;
}

int main(int argc, char **argv) {
   char *error = NULL;
   if (argc < 2) {
      printf("Please provide an input file\n");
      return 1;
   }
   LLVMModuleRef mod = create_module(argv[1]);
   LLVMLinkInMCJIT();
   LLVMInitializeNativeTarget();
   LLVMInitializeNativeAsmPrinter();
   LLVMMCJITMemoryManagerRef mm_ref = LLVMCreateSimpleMCJITMemoryManager(
            0,
            code_section_cb,
            data_section_cb,
            finalize_cb,
            destroy_cb
         );
   LLVMExecutionEngineRef ee;
   struct LLVMMCJITCompilerOptions options = {
      0, LLVMCodeModelDefault, 1, 0, mm_ref
   };
   LLVMCreateMCJITCompilerForModule(&ee, mod, &options, sizeof(options),
                                    &error);

   LLVMValueRef fun = LLVMGetNamedFunction(mod, "f");

   LLVMGenericValueRef args[] = {
       LLVMCreateGenericValueOfInt(LLVMInt32Type(), 10, 1)
   };

   LLVMRunFunction(ee, fun, 1, args);
   return 0;
}
