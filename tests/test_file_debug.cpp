#include "test_framework.h"
#include <unistd.h>

int main() {
    TestEditor te;
    te.type_text("Special: !@#$%^&*()");
    
    printf("[DEBUG] Before save - rope length: %zu\n", rope_length(&te.editor.rope));
    char* before_save = rope_to_string(&te.editor.rope);
    printf("[DEBUG] Before save - text: '%s'\n", before_save);
    delete[] before_save;
    
    editor_save_file(&te.editor, "/tmp/test_debug.txt");
    
    TestEditor te2;
    printf("[DEBUG] Before load - rope length: %zu\n", rope_length(&te2.editor.rope));
    
    editor_open_file(&te2.editor, "/tmp/test_debug.txt");
    
    printf("[DEBUG] After load - rope length: %zu\n", rope_length(&te2.editor.rope));
    char* after_load = rope_to_string(&te2.editor.rope);
    printf("[DEBUG] After load - text: '%s'\n", after_load);
    delete[] after_load;
    
    printf("[DEBUG] get_text() returns: '%s'\n", te2.get_text().c_str());
    
    return 0;
}
