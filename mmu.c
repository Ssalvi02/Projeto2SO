#include "globals.h"
#include "mmu.h"

void print_process_entry(process_entry pe)
{
    printf("%d %d\n", pe.op, pe.address);
}

table_entry *new_page_table(int table_size)
{  
    table_entry *page_table = (table_entry*)malloc(table_size * sizeof(table_entry));

    for(int i = 0; i < table_size; i++)
    {
        page_table[i].frame = 0;
        page_table[i].v = false;
        page_table[i].r = false;
        page_table[i].m = false;
    }

    return page_table;
}

void reset_table_entry(int pti)
{
    page_table[pti].v = false;
    page_table[pti].r = false;
    page_table[pti].m = false;
}

void print_table()
{
    printf(" --------------------------\n");
    printf("| PAGE TABLE               |\n");
    printf("|--------------------------|\n");
    printf("| page | v | r | m | frame |\n");
    for(int i = 0; i < total_virtual_pages; i++)
    {
        table_entry te = page_table[i];
        printf("| %4d | ", i);
        printf("%s | ", te.v ? "1" : " ");
        printf("%s | ", te.r ? "1" : " ");
        printf("%s | ", te.m ? "1" : " ");
        te.v ? printf("%5d |\n", te.frame) : printf("      |\n");
    }
    printf(" --------------------------\n");
}

void print_ram()
{
    printf(" --------------\n");
    printf("| RAM CONTENT  |\n");
    printf("|--------------|\n");
    printf("| frame | data |\n");
    for(int i = 0; i < ram_size; i++)
    {
        printf("| %5d | ", i/page_size);
        ram[i] == -1 ? printf("     |\n") : printf("%4d |\n", ram[i]);
    }
        
    printf(" --------------\n");
}

void ram_access(int pti, int logical_address, op_code op)
{
    int offset = OFFSET(logical_address);
    int ra = RAM_ADDRESS(page_table[pti].frame, offset);
    if(op == WRITE)
    {
        printf("write to %x\n", ra);
        page_table[pti].m = true;
    }
    printf("virtual address 0x%x -> physical address 0x%x\n\n", logical_address, ra);
}

int find_free_slot()
{
    for(int i = 0; i < ram_size; i+= page_size)
    {
        if(ram[i] == -1) return i;
    }

    return -1;
}

bool page_on_ram(int pti, int addr, op_code op)
{
    if(page_table[pti].v == false) return false;

    printf("page %d on ram\n", pti);
    
    ram_access(pti, addr, op);

    return true;
}

void update_ram(int logical_address, int physical_address)
{
    // acesso base do quadro e copio a partir dai
    int da = DISK_ADDRESS(logical_address);
    for(int i = da; i < da+page_size; i++)
    {
        ram[physical_address++] = disk[i];
    }
}

void update_table(int pti, int logical_address, op_code op)
{
    printf("added page %d to ram\n", pti);
    page_table[pti].v = true; // indico que agora a posicao eh valida
    page_table[pti].r = true; // indico que houve acesso

    ram_access(pti, logical_address, op);
}

int fifo_policy(int pti)
{
    int victim = queue_front(&fifo);
    queue_pop(&fifo);
    return victim;    
}

void simulation()
{
    for(int p = 0; p < process_len; p++)
    {
        // acesso tabela de paginas e verifico a validade
        op_code operation = entries[p].op;
        int addr = entries[p].address;
        int pti = PAGE_TABLE_INDEX(addr);

        print_table();
        print_ram();
        printf("total page faults: %d\n", page_fault_count);
        printf("\n--------------------- press key to continue\n");
        getchar();
        printf("op: %s | ", operation == READ ? "r" : "w");
        printf("address: %d\n", addr);


        // se for verdadeira, posso acessar a ram
        if(page_on_ram(pti, addr, operation)) continue;

        // atualizo a fila do fifo
        if(algorithm == FIFO) queue_push(&fifo, pti);

        printf("not on ram - page fault\n");
        page_fault_count++;

        // verifico se ha espaco livre
        int slot = find_free_slot();
        // se encontro, copio as informacoes do disco para a ram
        if(slot != -1)
        {
            printf("free space on ram\n");
            // indico que apartir de agora o frame se encontra no slot anteriormente vazio
            page_table[pti].frame = slot / page_size;
            update_table(pti, addr, operation);            
            update_ram(addr, slot);
                  
            continue;
        }

        // se nao ha free slot, preciso substituir um deles
        // escolhe a pagina vitima de acordo com a politica de substituicao
        int victim;
        switch(algorithm)
        {
            case FIFO: victim = fifo_policy(pti); break;
            case LRU: break;
            case NRU: break;
        }
        
        page_table[pti].frame = page_table[victim].frame;
        slot = page_table[victim].frame * page_size;
        printf("page %d was chosen to be removed\n", victim);
        if(page_table[victim].m)
            printf("page %d was modified, write to disk\n", victim);
        reset_table_entry(victim);
        update_table(pti, addr, operation);
        update_ram(addr, slot); 
    }
    print_table();
    print_ram();
    printf("total page faults: %d\n", page_fault_count);
}