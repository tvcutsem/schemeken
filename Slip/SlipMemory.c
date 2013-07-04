                     /*---------------------------------------------*/
                     /*                  >>>Slip<<<                 */
                     /*                 Theo D'Hondt                */
                     /*          VUB Software Languages Lab         */
                     /*                  (c) 2012                   */
                     /*---------------------------------------------*/
                     /*   version 13: completion and optimization   */
                     /*---------------------------------------------*/
                     /*                    memory                   */
                     /*---------------------------------------------*/

#include "SlipMemory.h"
#include "SlipPersist.h"

/*----------------------------------- private constants --------------------------------*/

enum { One_bit     = 0x0000000000000001 ,
       Two_bits    = 0x0000000000000003 ,
       Three_bits  = 0x0000000000000007 ,
       Six_bits    = 0x000000000000003F ,
       Three_bytes = 0x0000000000FFFFFF };

enum { Ptr_shift = 0,
       Imm_shift = 1,
       Bsy_shift = 1,
       Raw_shift = 2,
       Tag_shift = 2,
       Siz_shift = 8 };

enum { Imm_mask = One_bit<<Ptr_shift ,
       Bsy_mask = One_bit<<Bsy_shift ,
       Tag_mask = Six_bits<<Tag_shift ,
       Siz_mask = Three_bytes<<Siz_shift };

enum { bp_bits  = (0<<Ptr_shift) | (0<<Bsy_shift) ,
       bP_bits  = (1<<Ptr_shift) | (0<<Bsy_shift) ,
       Bp_bits  = (0<<Ptr_shift) | (1<<Bsy_shift) ,
       BP_bits  = (1<<Ptr_shift) | (1<<Bsy_shift) };

enum { rbp_bits = (0<<Ptr_shift) | (0<<Bsy_shift) | (0<<Raw_shift) ,
       rBp_bits = (0<<Ptr_shift) | (1<<Bsy_shift) | (0<<Raw_shift) ,
       rBP_bits = (1<<Ptr_shift) | (1<<Bsy_shift) | (0<<Raw_shift) ,
       Rbp_bits = (0<<Ptr_shift) | (0<<Bsy_shift) | (1<<Raw_shift) ,
       RBp_bits = (0<<Ptr_shift) | (1<<Bsy_shift) | (1<<Raw_shift) ,
       RBP_bits = (1<<Ptr_shift) | (1<<Bsy_shift) | (1<<Raw_shift) };

/*------------------------------------- opaque types -----------------------------------*/

typedef union PTR { CEL_type cel;
                    PTR_type ptr; } PTR;

/*----------------------------------- private variables --------------------------------*/

static PTR_type Free;
static PTR_type Head;
static PTR_type Tail;

static UNS_type Collected;
static UNS_type Count;

static BYT_type Tag_boundary;

/*----------------------------------- private functions --------------------------------*/

static CEL_type busy_pointer(PTR_type Pointer)
  { return ((CEL_type)Pointer | Bsy_mask); }

static CEL_type busy_cell(CEL_type Cell)
  { return (Cell | Bsy_mask); }

static PTR_type free_cell(CEL_type Cell)
  { return (PTR_type)(Cell & ~Bsy_mask); }

static CEL_type free_size(UNS_type Size)
  { return (CEL_type)(Size << Siz_shift); }

static SGN_type get_immediate(PTR_type Pointer)
  { return ((SGN_type)Pointer >> Imm_shift); }

static BYT_type get_last_2_bits(CEL_type Cell)
  { return (Cell & Two_bits); }

static UNS_type get_size(CEL_type Cell)
  { return ((UNS_type)Cell >> Siz_shift); }

static BYT_type get_tag(CEL_type Cell)
  { return ((UNS_type)(Cell & Tag_mask) >> Tag_shift); }

static BYT_type is_busy(CEL_type Cell)
  { return ((BYT_type)(Cell & Bsy_mask) >> Bsy_shift); }

static BYT_type is_immediate(PTR_type Pointer)
  { return ((CEL_type)Pointer & Imm_mask); }

static CEL_type make_header(BYT_type Tag,
                            UNS_type Size)
  { return (CEL_type)((Size << Siz_shift) | (Tag << Tag_shift )); }

static PTR_type make_immediate(UNS_type Signed)
  { CEL_type cell;
    cell = ((CEL_type)Signed << Imm_shift) | Imm_mask;
    return (PTR_type) cell; }

static NIL_type sweep_and_mark(NIL_type)
  { BYT_type tag;
    CEL_type cell,
             header;
    PTR_type current,
             pointer;
    UNS_type size;
    for (current = Head;;)
      { cell = current->cel;                    /* <current> points to <cell> */
        switch (get_last_2_bits(cell))               /* last 2 bits of <cell> */
          { case bp_bits:                              /* <cell> is a pointer */
              pointer = current->ptr;        /* <current> points to <pointer> */
              if ((pointer <  Head) ||           /* <pointer> is inline value */
                  (pointer >= Tail))                   /* or a native pointer */
                { current--;                             /* advance <current> */
                  break; }
              header = pointer->cel;          /* <pointer> points to <header> */
              switch (get_last_2_bits(header))     /* last 2 bits of <header> */
                { case bp_bits:                  /* <header> is a free header */
                    pointer->cel = busy_pointer(current);             /* swap */
                    current->cel = header;           /*<header> and <pointer> */
                    tag = get_tag(header);
                    if (tag <= Tag_boundary)  /* <header> is a non-raw header */
                      { size = get_size(header);
                        if (size > 0)                   /* if chunk not empty */
                          current = pointer + size;         /* make <current> */
                        break; }                             /* point to last */
                    current--;                           /* advance <current> */
                    break;
                  case Bp_bits:                  /* <header> is a busy header */
                    pointer = free_cell(header);
                    header = pointer->cel;
                    pointer->cel = busy_pointer(current);     /* mark current */
                    current->cel = header;
                    current--; }                         /* advance <current> */
              break;
            case Bp_bits:                          /* <cell> is a busy header */
              current = free_cell(cell);     /* restore <current> from <cell> */
              if (current == Head)          /* if backtracked to root pointer */
                return;
            case bP_bits:                       /* <cell> is an inline number */
            case BP_bits:
              current--; }}}                             /* advance <current> */

static NIL_type traverse_and_update(NIL_type)
  { CEL_type cell;
    PTR_type destination,
             free,
             pointer,
             source;
    UNS_type accumulated_size,
             size;
    source = destination = Head + 1;
    while (source < Free)
      { cell = source->cel;
        if (is_busy(cell))
          { do
              { pointer = free_cell(cell);
                cell = pointer->cel;
                pointer->ptr = destination; }
            while (is_busy(cell));
            source->cel = busy_cell(cell);
            size = get_size(cell) + 1;
            source += size;
            destination += size; }
        else
          { free = source;
            accumulated_size = 0;
            do
              { size = get_size(cell) + 1;
                if ((accumulated_size + size) >= Three_bytes)
                  break;
                accumulated_size += size;
                source += size;
                if (source >= Free)
                  break;
                cell = source->cel; }
            while (!is_busy(cell));
            free->cel = free_size(accumulated_size - 1); }}}

static NIL_type traverse_and_compact(NIL_type)
  { CEL_type cell;
    PTR_type destination,
             source;
    UNS_type size;
    source = destination = Head + 1;
    while (source < Free)
      { cell = (source++)->cel;
        size = get_size(cell);
        if (is_busy(cell))
          { (destination++)->ptr = free_cell(cell);
            while (size--)
              (destination++)->cel = (source++)->cel; }
        else
          source += size; }
    Free = destination; }

static NIL_type sweep_and_replace(PTR_type Original,
                                  PTR_type Replacement)
  { BYT_type tag;
    CEL_type cell,
             header;
    PTR_type current,
             pointer;
    UNS_type size;
    for (current = Head;;)
      { cell = current->cel;                    /* <current> points to <cell> */
        switch (get_last_2_bits(cell))               /* last 2 bits of <cell> */
          { case bp_bits:                              /* <cell> is a pointer */
              pointer = current->ptr;        /* <current> points to <pointer> */
              if (pointer < Head)                /* <pointer> is inline value */
                { current--;                             /* advance <current> */
                  break; }
              header = pointer->cel;          /* <pointer> points to <header> */
              switch (get_last_2_bits(header))     /* last 2 bits of <header> */
                { case bp_bits:                  /* <header> is a free header */
                    tag = get_tag(header);
                    if (tag <= Tag_boundary)  /* <header> is a non-raw header */
                      { size = get_size(header);
                        if (size > 0)                   /* if chunk not empty */
                          { pointer->cel = busy_pointer(current);     /* swap */
                            current->cel = header;  /* <header> and <pointer> */
                            current = pointer + size;       /* make <current> */
                            break; }}                        /* point to last */
                    pointer->cel = busy_cell(header);          /* mark header */
                    if (pointer == Original)
                      current->ptr = Replacement;
                    current--;                           /* advance <current> */
                    break;
                  case Bp_bits:                  /* <header> is a busy header */
                    if (pointer == Original)
                      current->ptr = Replacement;
                    current--; }                         /* advance <current> */
              break;
            case Bp_bits:                          /* <cell> is a marked cell */
              pointer = free_cell(cell);       /* <pointer> is <cell> content */
              header = pointer->cel;          /* <pointer> points to <header> */
              current->cel = busy_cell(header);    /* restore original header */
              if (current == Original)
                pointer->ptr = Replacement;
              else
                pointer->ptr = current;           /* restore original pointer */
              if (pointer == Head)          /* if backtracked to root pointer */
                return;
              current = pointer - 1;                     /* advance <current> */
              break;
            case bP_bits:                       /* <cell> is an inline number */
            case BP_bits:
              current--; }}}                             /* advance <current> */

static NIL_type traverse_and_unmark(NIL_type)
  { CEL_type header;
    PTR_type source;
    UNS_type size;
    source = Head + 1;
    while (source < Free)
      { header = source->cel;
        if (is_busy(header))
          source->ptr = free_cell(header);
        size = get_size(header);
        source += size + 1; }}

/*----------------------------------- public functions ---------------------------------*/

UNS_type Memory_Available(NIL_type)
  { return (Tail - Free); }

NIL_type Memory_Collect_C(NIL_type)
  { PTR_type old_free,
             root;
    Slip_Log("garbage collect");
    Count += 1;
    old_free = Free;
    root = Memory_Before_Reclaim();
    Head->ptr = root;
    sweep_and_mark();
    traverse_and_update();
    traverse_and_compact();
    root = Head->ptr;
    Collected += old_free - Free;
    Memory_After_Reclaim(root); }

UNS_type  Memory_Collect_Count(NIL_type)
  { return Count; }
  
DBL_type Memory_Consumption(NIL_type)
  { DBL_type consumption;
    consumption = Collected;
    consumption += Free - Head;
    return consumption; }

SGN_type Memory_Get_Immediate(PTR_type Pointer)
  { return get_immediate(Pointer); }

UNS_type Memory_Get_Size(PTR_type Pointer)
  { CEL_type cell;
    cell = Pointer->cel;
    return get_size(cell); }

BYT_type Memory_Get_Tag(PTR_type Pointer)
  { CEL_type cell;
    cell = Pointer->cel;
    return get_tag(cell); }

BYT_type Memory_Is_Immediate(PTR_type Pointer)
  { return is_immediate(Pointer); }

PTR_type Memory_Make_Chunk_M(BYT_type Tag,
                             UNS_type Size)
  { PTR_type pointer;
    pointer = Free;
    Free += Size + 1;
    pointer->cel = make_header(Tag,
                               Size);
    return pointer; }

PTR_type Memory_Make_Immediate(UNS_type Long)
  { return make_immediate(Long); }

NIL_type Memory_Replace(PTR_type Old_pointer,
                        PTR_type New_pointer)
  { PTR_type root;
    root = Memory_Before_Reclaim();
    Head->ptr = root;
    sweep_and_replace(Old_pointer,
                      New_pointer);
    traverse_and_unmark();
    root = Head->ptr;
    Memory_After_Reclaim(root); }

NIL_type Memory_Set_Tag(PTR_type Pointer,
                        BYT_type Tag)
  { UNS_type size;
    size = Pointer->cel >> Siz_shift;
    Pointer->cel = make_header(Tag,
                               size); }

/*--------------------------------------------------------------------------------------*/

NIL_type Memory_Initialize(ADR_type Address,
                           UNS_type Size,
                           BYT_type Boundary)
  { slipken_persist_init(Collected, 0);
    slipken_persist_init(Count, 0);
    slipken_persist_init(Head, (PTR_type)Address);
    slipken_persist_init(Free, Head + 1);
    slipken_persist_init(Tail, Head + Size);
    slipken_persist_init(Tag_boundary, Boundary); }
