#include "ast.h"
#include "utils.h"
#include "splcdef.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int splcf_ast_dump = 0;

int splcf_enable_colored_ast = 0;

int splcf_enable_ast_punctuators = 0;

ast_node ast_create_empty_node()
{
    ast_node node = (ast_node)malloc(sizeof(ast_node_struct));
    SPLC_ALLOC_PTR_CHECK(node, "out of memory");
    node->type = SPLT_NULL;
    node->symtable = NULL;
    node->children = NULL;
    node->num_child = 0;
    memset(&node->location, 0, sizeof(splc_loc));
    node->location.fid = -1;
    return node;
}

ast_node ast_create_leaf_node(const splc_token_t type, const splc_loc location)
{
    ast_node node = ast_create_empty_node();
    node->type = type;
    node->location = location;
    return node;
}

ast_node ast_add_child(ast_node parent, ast_node child)
{
    if (SPLC_AST_IGNORE_NODE(child))
        return parent;

    parent->children = (ast_node *)realloc(parent->children, (parent->num_child + 1) * sizeof(ast_node));
    SPLC_ALLOC_PTR_CHECK(parent->children, "out of memory");
    parent->children[parent->num_child] = child;
    ++(parent->num_child);
    return parent;
}

ast_node ast_add_children(ast_node parent, size_t num_child, ...)
{
    if (num_child > 0)
    {
        va_list args;
        va_start(args, num_child);

        for (size_t i = 0; i < num_child; ++i)
        {
            ast_node child = va_arg(args, ast_node);
            if (SPLC_IS_LOC_INVALID(parent->location))
            {
                parent->location = child->location;
            }
            else if (parent->location.fid == child->location.fid)
            {
                parent->location.lineend = child->location.lineend;
                parent->location.colend = child->location.colend;
            }
            if (SPLC_AST_IGNORE_NODE(child))
                continue;
            ast_add_child(parent, child);
        }
        va_end(args);
    }
    return parent;
}

ast_node ast_create_parent_node(const splc_token_t type, size_t num_child, ...)
{
    ast_node node = ast_create_empty_node();
    node->type = type;
    node->location = SPLC_INVALID_LOC;
    if (num_child > 0)
    {
        va_list args;
        va_start(args, num_child);

        for (size_t i = 0; i < num_child; ++i)
        {
            ast_node child = va_arg(args, ast_node);
            if (SPLC_IS_LOC_INVALID(node->location))
            {
                node->location = child->location;
            }
            else if (node->location.fid == child->location.fid)
            {
                node->location.lineend = child->location.lineend;
                node->location.colend = child->location.colend;
            }
            if (SPLC_AST_IGNORE_NODE(child))
                continue;
            ast_add_child(node, child);
        }
        va_end(args);
    }
    return node;
}

splc_loc ast_get_startloc(const ast_node node)
{
    if (node->num_child == 0)
        return SPLC_MAKE_LOC(node->location.fid, node->location.linebegin, node->location.colbegin,
                             node->location.linebegin, node->location.colbegin);
    else
        return ast_get_startloc(node->children[0]);
}

splc_loc ast_get_endloc(const ast_node node)
{
    if (node->num_child == 0)
        return SPLC_MAKE_LOC(node->location.fid, node->location.lineend, node->location.colend, node->location.lineend,
                             node->location.colend);
    else
        return ast_get_endloc(node->children[node->num_child - 1]);
}

void ast_release_node(ast_node *root)
{
    if (root == NULL || (*root) == NULL)
        return;
    for (int i = 0; i < (*root)->num_child; ++i)
    {
        ast_release_node(&(*root)->children[i]);
    }
    free((*root)->children);
    lut_free_table(&((*root)->symtable));
    if (SPLT_AST_REQUIRE_VAL_FREE((*root)->type))
        free((*root)->val);
    free(*root);
    *root = NULL;
}

void ast_preprocess(ast_node root)
{
    // TODO: remove all punctuators
    SPLC_ASSERT(root != NULL);
    size_t new_nchild = 0;
    for (size_t i = 0; i < root->num_child; ++i)
    {
        if (SPLT_IS_PUNCTUATOR(root->children[i]->type))
        {
            ast_release_node(&root->children[i]);
        }
        else
        {
            if (new_nchild != i)
                root->children[new_nchild] = root->children[i];
            ast_preprocess(root->children[i]);
            new_nchild++;
        }
    }
    ast_node *newarray = (ast_node *)realloc(root->children, new_nchild * sizeof(ast_node));
    if (new_nchild >= 1)
        SPLC_ALLOC_PTR_CHECK(newarray, "failed to preprocess node: out of memory");
    root->children = newarray;
    root->num_child = new_nchild;
}

ast_node ast_deep_copy(ast_node node)
{
    if (node == NULL)
        return NULL;

    ast_node result = ast_create_empty_node();
    result->type = node->type;
    result->symtable = lut_copy_table(node->symtable);
    result->location = node->location;
    if (node->num_child > 0)
    {
        result->num_child = node->num_child;
        result->children = (ast_node *)malloc(result->num_child * sizeof(ast_node));
        SPLC_ALLOC_PTR_CHECK(result->children, "failed to copy node: out of memory");
        for (int i = 0; i < result->num_child; ++i)
        {
            result->children[i] = ast_deep_copy(node->children[i]);
        }
    }
    if (SPLT_IS_VAL_ALLOCATED(node->type))
    {
        switch (node->type)
        {
        case SPLT_STR_UNIT:
            result->val = (void *)strdup((char *)node->val);
            SPLC_ALLOC_PTR_CHECK(result->val, "failed to copy string to another node");
            break;
        default:
            SPLC_FWARN_NOLOC("AST value cannot be copied due to undefined behavior on node type: %s",
                             splc_token2str(node->type));
            break;
        }
    }
    else
    {
        result->ull_val = node->ull_val;
    }

    return result;
}

static void _builtin_print_single_node(const ast_node node)
{
    // print node type
    SPLC_AST_PRINT_COLORED("\033[1m");
    SPLC_AST_PRINT_COLORED(splc_get_token_color_code(node->type));

    const char *tokenstr = splc_token2str(node->type); /* do not free this string, as it is a constant */
    printf("%s", tokenstr);

    SPLC_AST_PRINT_COLORED("\033[0m");

    // print node location
    printf(" <");
    SPLC_AST_PRINT_COLORED(SPLC_IS_LOC_INVALID(node->location) ? "\033[31m" : "\033[33m");

    char *location = splc_loc2str(node->location);
    printf("%s", location);
    free(location);

    SPLC_AST_PRINT_COLORED("\033[0m");
    printf(">");

    // Print node content
    
    // Print node symbol table state
    if (node->symtable != NULL)
    {
        printf(" <");
        SPLC_AST_PRINT_COLORED("\033[36m");
        char *symstr = lut_get_info_string(node->symtable);
        printf("%s", symstr);
        free(symstr);
        SPLC_AST_PRINT_COLORED("\033[0m");
        printf(">");
    }

    // Print node value based on its type
    const char *ast_color_construct = "\033[96m";
    const char *ast_color_constant = "\033[32m";

    switch (node->type)
    {
    case SPLT_TRANS_UNIT:
    case SPLT_ID:
    case SPLT_TYPEDEF_NAME:
        SPLC_AST_PRINT_COLORED(ast_color_construct);
        break;
    case SPLT_LTR_INT:
    case SPLT_LTR_FLOAT:
    case SPLT_LTR_CHAR:
    case SPLT_STR_UNIT:
        SPLC_AST_PRINT_COLORED(ast_color_constant);
        break;
    default:
        break;
    }

    switch (node->type)
    {
    case SPLT_TRANS_UNIT: {
        printf("  <%s>", splc_get_node_filename(node->location.fid));
        break;
    }
    case SPLT_ID: {
        printf("  %s", (char *)node->val);
        break;
    }
    case SPLT_TYPEDEF_NAME: {
        printf("  %s", (char *)node->val);
        break;
    }
    case SPLT_LTR_INT: {
        printf("  '%llu'", node->ull_val);
        break;
    }
    case SPLT_LTR_FLOAT: {
        printf("  '%g'", node->float_val);
        break;
    }
    case SPLT_LTR_CHAR: {
        printf("  '%s'", (char *)node->val);
        break;
    }
    case SPLT_STR_UNIT: {
        printf("  \"%s\"", (char *)node->val);
        break;
    }
    default:
        break;
    }
    SPLC_AST_PRINT_COLORED("\033[0m");
}

static void _builtin_ast_print(const ast_node node, const char *prefix)
{
    const char *indicator = NULL;
    const char *segment = NULL;

    for (int i = 0; i < node->num_child; i++)
    {
        if (i == node->num_child - 1)
        {
            indicator = "`-";
            segment = "  ";
        }
        else
        {
            indicator = "|-";
            segment = "| ";
        }

        SPLC_AST_PRINT_COLORED("\033[34m");
        printf("%s%s", prefix, indicator);
        SPLC_AST_PRINT_COLORED("\033[0m");

        _builtin_print_single_node(node->children[i]);
        printf("\n");

        if (node->children[i]->num_child > 0)
        {
            char *next_prefix = (char *)malloc((strlen(prefix) + strlen(segment) + 1) * sizeof(char));
            sprintf(next_prefix, "%s%s", prefix, segment);
            _builtin_ast_print(node->children[i], next_prefix);
            free(next_prefix);
        }
    }
}

void ast_print(ast_node root)
{
    if (root != NULL)
    {
        _builtin_print_single_node(root);
        printf("\n");
        _builtin_ast_print(root, "");
    }
    else
    {
        printf("%s", splc_get_token_color_code(SPLT_NULL));
        printf("(%s)\033[0m\n", splc_token2str(SPLT_NULL));
    }
}

void ast_sem_search(ast_node node, splc_trans_unit tunit, int new_sym_table, splc_entry_t decl_entry_type, const char* decl_spec_type)
{
    // new table construction
    int find_stmt = 0;
    if((node->type == SPLT_STRUCT_UNION_SPEC && node->num_child == 3) || node->type == SPLT_FUNC_DEF)
        new_sym_table = 1;
    if(new_sym_table)
        splc_push_symtable(tunit, 0);
        
    int copy_new_sym_table = new_sym_table;
    if(node->type == SPLT_SEL_STMT || node->type == SPLT_ITER_STMT)
        find_stmt = 1;
    
    // definition
        //definition of struct/union
    if(node->type == SPLT_STRUCT_UNION_SPEC && node->num_child == 3)
    {
        ast_node type_children = node->children[0];
        ast_node id_children = node->children[1];
        splc_entry_t tmp_decl_entry_type;
        if(type_children->type == SPLT_KWD_STRUCT)  tmp_decl_entry_type = SPLE_STRUCT_DEC;
        else{
            tmp_decl_entry_type = SPLE_UNION_DEC;
        }
        char* struct_union_name = (char*)(id_children->val); // name of struct/union
        //TODO: check if there is a struct(or function) with the same name
        lut_insert(tunit->envs[(tunit->nenvs)-1], struct_union_name, tmp_decl_entry_type, NULL, node, node->location);
    }
        // definition in struct/union
    if(node->type == SPLT_STRUCT_DECLTN) // Struct/Union-Decl
    {
        ast_node typespec_child_node = ((node->children[0])->children[0])->children[0]; // son of TypeSpec
        if(typespec_child_node->num_child == 0)  // variable type
        {
            decl_entry_type = SPLE_VAR;
            decl_spec_type = splc_token2str(typespec_child_node->type);
        }
        else{ // struct/union type
            // pass two parameter into his brothers for inserting into table
            ast_node type_children = typespec_child_node->children[0];
            ast_node id_children = typespec_child_node->children[1];
            if(type_children->type == SPLT_KWD_STRUCT)  decl_entry_type = SPLE_STRUCT_DEC;
            else{
                decl_entry_type = SPLE_UNION_DEC;
            }
            decl_spec_type = (char*)(id_children->val);

            // insert into table if it is a new struct
            if(typespec_child_node->num_child == 3)
            {
                lut_insert(tunit->envs[(tunit->nenvs)-1], decl_spec_type, decl_entry_type, NULL, typespec_child_node, typespec_child_node -> location);
            }
        }
    }
        // definition of function
    if(node->type == SPLT_FUNC_DEF)  //FunctionDef
    {
        ast_node typespec_child_node = (((node->children[0])->children[0])->children);
        
    }
    
    // search children
    for(int i = 0; i < node->num_child; i++)
    {
        // get new_sym_table
        ast_node child = node->children[i];
        if(child->type == SPLT_STMT && find_stmt){
            new_sym_table = 1;
            find_stmt = 0;
        }
        else{
            new_sym_table = 0;
        }
        
        //iteration
        ast_sem_search(child, tunit, new_sym_table, decl_entry_type, decl_spec_type);
    }

    // pop symbol table and link it to the node
    if(copy_new_sym_table)
    {
        lut_table top_sym_table = splc_pop_symtable(tunit);
        node->symtable = top_sym_table;
    }
}