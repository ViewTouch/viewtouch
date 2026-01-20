/*
 * list_utility.hh - revision 6 (8/10/98)
 * linked list template classes
 *
 * SList - single linked list
 *   required fields for items: next // see note below
 *
 * DList - double linked list
 *   required fields for items: fore, next // see note below
 *
 *			YIKES!!  Explicit reference to members of target structure
 *			(fore and next) is a very unsafe departure from the
 *			template/abstraction approach and constrains these list
 *			mechanisms to types that contain these members.
 *
 *			This needs to be fixed.
 */

#ifndef VT_LIST_UTILITY_HH
#define VT_LIST_UTILITY_HH

#include "basic.hh"
#include "fntrace.hh"
#include "utility.hh"
#include <utility>

/**** Types ****/
template <typename T>
class SList
{
    T *list_head{nullptr};
    T *list_tail{nullptr};
    int cached_count{0};  // Cached count for O(1) Count()

public:
    // Constructors
    SList() = default;
    explicit SList(T *item) : list_head(item), list_tail(item), cached_count(item ? 1 : 0) {}
    
    // Delete copy operations (use move instead)
    SList(const SList&) = delete;
    SList& operator=(const SList&) = delete;
    
    // Move operations
    SList(SList&& other) noexcept 
        : list_head(other.list_head)
        , list_tail(other.list_tail)
        , cached_count(other.cached_count)
    {
        other.list_head = nullptr;
        other.list_tail = nullptr;
        other.cached_count = 0;
    }
    
    SList& operator=(SList&& other) noexcept
    {
        if (this != &other)
        {
            Purge();
            list_head = other.list_head;
            list_tail = other.list_tail;
            cached_count = other.cached_count;
            other.list_head = nullptr;
            other.list_tail = nullptr;
            other.cached_count = 0;
        }
        return *this;
    }

    // Destructor
    ~SList() { Purge(); }

    // Member Functions
    [[nodiscard]] T *Head() noexcept { return list_head; }
    [[nodiscard]] T *Tail() noexcept { return list_tail; }
    [[nodiscard]] const T *Head() const noexcept { return list_head; }
    [[nodiscard]] const T *Tail() const noexcept { return list_tail; }

    [[nodiscard]] bool IsEmpty() const noexcept
    {
        return (list_head == nullptr);
    }

    int AddToHead(T *item) noexcept
    {
        FnTrace("SList::AddToHead()");
        if (item == nullptr)
            return 1;

        item->next = list_head;

        if (list_tail == nullptr)
            list_tail = item;

        list_head = item;
        ++cached_count;
        return 0;
    }

    int AddToTail(T *item) noexcept
    {
        FnTrace("SList::AddToTail()");
        if (item == nullptr)
            return 1;

        item->next = nullptr;

        if (list_tail)
            list_tail->next = item;
        else
            list_head = item;
        list_tail = item;
        ++cached_count;
        return 0;
    }

    int AddAfterNode(T *node, T *item) noexcept
    {
        FnTrace("SList::AddAfterNode()");
        if (node == nullptr)
            return AddToHead(item);
        if (node == list_tail)
            return AddToTail(item);

        item->next = node->next;
        node->next = item;
        ++cached_count;
        return 0;
    }

    void Purge() noexcept
    {
        FnTrace("SList::Purge()");
        while (list_head)
        {
            T *tmp = list_head;
            list_head = list_head->next;
            delete tmp;
        }
        list_tail = nullptr;
        cached_count = 0;
    }

    int Remove(T *node) noexcept
    {
        FnTrace("SList::Remove()");
        if (node == nullptr)
            return 1;
        for (T *n = list_head, *p = nullptr; n != nullptr; p = n, n = n->next)
            if (node == n)
            {
                if (p)
                    p->next = node->next;
                else
                    list_head = node->next;
                if (list_tail == node)
                    list_tail = p;
                node->next = nullptr;
                --cached_count;
                return 0;
            }
        return 1;
    }

    /**
     * @brief Removes item from list AND deletes it.
     * @param item The item to remove and delete
     * @return 0 on success, 1 on failure
     * @note Use this when the SList owns the item's memory.
     *       Use Remove() alone when ownership is transferred elsewhere.
     */
    int RemoveAndDelete(T *item) noexcept
    {
        FnTrace("SList::RemoveAndDelete()");
        if (item == nullptr)
            return 1;
        
        int result = Remove(item);
        if (result == 0)
            delete item;
        return result;
    }

    [[nodiscard]] int Count() const noexcept
    {
        FnTrace("SList::Count()");
        return cached_count;  // O(1) instead of O(n)
    }

    [[nodiscard]] T *Index(int i) noexcept
    {
        FnTrace("SList::Index()");
        if (i < 0)
            return nullptr;

        T *n = list_head;
        while (n != nullptr && i > 0)
            --i, n = n->next;
        return n;
    }

    // Operators
    [[nodiscard]] T *operator[](int i) noexcept { return Index(i); }
};

template <typename T>
class DList
{
    T *list_head{nullptr};
    T *list_tail{nullptr};
    int cached_count{0};  // Cached count for O(1) Count()
    
    T *InternalSort(T *list, int (*cmp)(T *, T *)) noexcept
    {
        FnTrace("DList::InternalSort()");
        T *p, *q, *e, *tail = nullptr;
        int insize, nmerges, psize, qsize, i;
        
        if (list == nullptr)
            return nullptr;

        insize = 1;
        
        while (true)
        {
            p = list;
            list = nullptr;
            tail = nullptr;
            
            nmerges = 0;  /* count number of merges we do in this pass */
            
            while (p)
            {
                nmerges++;  /* there exists a merge to be done */
                /* step `insize' places along from p */
                q = p;
                psize = 0;
                for (i = 0; i < insize; i++)
                {
                    psize++;
                    q = q->next;
                    if (q == nullptr)
                        break;
                }
                
                /* if q hasn't fallen off end, we have two lists to merge */
                qsize = insize;
                
                /* now we have two lists; merge them */
                while (psize > 0 || (qsize > 0 && q))
                {
                    /* decide whether next element of merge comes from p or q */
                    const bool take_from_q = (psize == 0)
                                           ? true
                                           : ! (qsize == 0 || q == nullptr || cmp(p, q) <= 0);

                    if (take_from_q)
                    {
                        /* take from q */
                        e = q; q = q->next; qsize--;
                    }
                    else
                    {
                        /* take from p */
                        e = p; p = p->next; psize--;
                    }
                    
                    /* add the next element to the merged list */
                    if (tail) {
                        tail->next = e;
                    } else {
                        list = e;
                    }
                    e->fore = tail;
                    tail = e;
                }
                
                /* now p has stepped `insize' places along, and q has too */
                p = q;
            }
            if (tail != nullptr) {
                tail->next = nullptr;
            }
            
            /* If we have done only one merge, we're finished. */
            if (nmerges <= 1)   /* allow for nmerges==0, the empty list case */
                return list;
            
            /* Otherwise repeat, merging lists twice the size */
            insize *= 2;
        }
    }
    
public:
    // Constructors
    DList() = default;
    explicit DList(T *item) : list_head(item), list_tail(item), cached_count(item ? 1 : 0) {}
    
    // Delete copy operations
    DList(const DList&) = delete;
    DList& operator=(const DList&) = delete;
    
    // Move operations
    DList(DList&& other) noexcept 
        : list_head(other.list_head)
        , list_tail(other.list_tail)
        , cached_count(other.cached_count)
    {
        other.list_head = nullptr;
        other.list_tail = nullptr;
        other.cached_count = 0;
    }
    
    DList& operator=(DList&& other) noexcept
    {
        if (this != &other)
        {
            Purge();
            list_head = other.list_head;
            list_tail = other.list_tail;
            cached_count = other.cached_count;
            other.list_head = nullptr;
            other.list_tail = nullptr;
            other.cached_count = 0;
        }
        return *this;
    }

    // Destructor
    ~DList()
    {
        FnTrace("DList::~DList()");
        Purge();
    }

    // Member Functions
    [[nodiscard]] T *Head() noexcept { return list_head; }
    [[nodiscard]] T *Tail() noexcept { return list_tail; }
    [[nodiscard]] const T *Head() const noexcept { return list_head; }
    [[nodiscard]] const T *Tail() const noexcept { return list_tail; }

    [[nodiscard]] bool IsEmpty() const noexcept
    {
        return (list_head == nullptr);
    }

    int AddToHead(T *item) noexcept
    {
        FnTrace("DList::AddToHead()");
        if (item == nullptr)
            return 1;

        item->fore = nullptr;
        item->next = list_head;
        if (list_head)
            list_head->fore = item;
        else
            list_tail = item;
        list_head = item;
        ++cached_count;
        return 0;
    }

    int AddToTail(T *item) noexcept
    {
        FnTrace("DList::AddToTail()");
        if (item == nullptr)
            return 1;

        item->fore = list_tail;
        item->next = nullptr;
        if (list_tail)
            list_tail->next = item;
        else
            list_head = item;
        list_tail = item;
        ++cached_count;
        return 0;
    }

    int AddAfterNode(T *node, T *item) noexcept
    {
        FnTrace("DList::AddAfterNode()");
        if (node == nullptr)
            return AddToHead(item);
        if (node == list_tail)
            return AddToTail(item);

        item->fore = node;
        item->next = node->next;
        node->next->fore = item;
        node->next = item;
        ++cached_count;
        return 0;
    }

    int AddBeforeNode(T *node, T *item) noexcept
    {
        FnTrace("DList::AddBeforeNode()");
        if (node == nullptr)
            return AddToTail(item);
        if (node == list_head)
            return AddToHead(item);

        item->next = node;
        item->fore = node->fore;
        item->fore->next = item;
        node->fore = item;
        ++cached_count;
        return 0;
    }

    [[nodiscard]] bool Exists(T *item, int (*cmp)(T *, T*)) noexcept
    {
        FnTrace("DList::Exists()");
        if (item == nullptr)
            return false;

        T *curr = list_head;

        while (curr != nullptr)
        {
            if (cmp(item, curr) == 0)
                return true;
            curr = curr->next;
        }

        return false;
    }

    int Remove(T *item) noexcept
    {
        FnTrace("DList::Remove()");
        if (item == nullptr)
            return 1;

        if (list_head == item)
            list_head = item->next;
        if (list_tail == item)
            list_tail = item->fore;
        if (item->next)
            item->next->fore = item->fore;
        if (item->fore)
            item->fore->next = item->next;
        item->fore = nullptr;
        item->next = nullptr;
        --cached_count;
        return 0;
    }

    int RemoveSafe(T *node) noexcept
    {
        FnTrace("DList::RemoveSafe()");
        if (node == nullptr)
            return 1;
        for (T *n = list_head; n != nullptr; n = n->next)
            if (n == node)
                return Remove(node);
        return 1;
    }

    /**
     * @brief Removes item from list AND deletes it.
     * @param item The item to remove and delete
     * @return 0 on success, 1 on failure
     * @note Use this when the DList owns the item's memory.
     *       Use Remove() alone when ownership is transferred elsewhere.
     */
    int RemoveAndDelete(T *item) noexcept
    {
        FnTrace("DList::RemoveAndDelete()");
        if (item == nullptr)
            return 1;
        
        int result = Remove(item);
        if (result == 0)
            delete item;
        return result;
    }

    void Purge() noexcept
    {
        FnTrace("DList::Purge()");

        while (list_head)
        {
            T *tmp = list_head;
            list_head = list_head->next;
            delete tmp;
        }
        list_tail = nullptr;
        cached_count = 0;
    }

    [[nodiscard]] int Count() const noexcept
    {
        FnTrace("DList::Count()");
        return cached_count;  // O(1) instead of O(n)
    }

    [[nodiscard]] T *Index(int i) noexcept
    {
        FnTrace("DList::Index()");
        if (i < 0)
            return nullptr;

        T *n = list_head;
        while (n != nullptr && i > 0)
        {
            --i;
            n = n->next;
        }
        return n;
    }

    int Sort(int (*cmp)(T *, T *)) noexcept
    {
        FnTrace("DList::Sort()");
        list_head = InternalSort(list_head, cmp);
        list_tail = list_head;
        if (list_tail != nullptr)
        {
            while (list_tail->next != nullptr)
                list_tail = list_tail->next;
        }
        return 0;
    }

    // Operators
    [[nodiscard]] T *operator[](int i) noexcept { return Index(i); }
};

#endif
