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

#pragma once  // REFACTOR: Replaced #ifndef VT_LIST_UTILITY_HH guard with modern pragma once

#include "basic.hh"
#include "fntrace.hh"
#include "utility.hh"

/**** Types ****/
template <class type>
class SList
{
    type *list_head, *list_tail;

public:
    // Constructors
    SList()           { list_head = nullptr; list_tail = nullptr; }  // REFACTOR: Changed NULL to nullptr for modern C++
    SList(type *item) { list_head = item; list_tail = item; }

    // Destructor
    ~SList() { Purge(); }

    // Member Functions
    type *Head() { return list_head; }
    type *Tail() { return list_tail; }

    int IsEmpty()
    {
        return (list_head == nullptr);  // REFACTOR: Changed NULL to nullptr for modern C++
    }

    int AddToHead(type *item)
    {
        FnTrace("SList::AddToHead()");
        if (item == nullptr)            // REFACTOR: Changed NULL to nullptr for modern C++
            return 1;

        item->next = list_head;

        if (list_tail == nullptr)       // REFACTOR: Changed NULL to nullptr for modern C++
            list_tail = item;

        list_head = item;
        return 0;
    }

    int AddToTail(type *item)
    {
        FnTrace("SList::AddToTail()");
        if (item == nullptr)        // REFACTOR: Changed NULL to nullptr for modern C++
            return 1;

        item->next = nullptr;       // REFACTOR: Changed NULL to nullptr for modern C++

        if (list_tail)
            list_tail->next = item;
        else
            list_head = item;
        list_tail = item;
        return 0;
    }

    int AddAfterNode(type *node, type *item)
    {
        FnTrace("SList::AddAfterNode()");
        if (node == nullptr)        // REFACTOR: Changed NULL to nullptr for modern C++
            return AddToHead(item);
        if (node == list_tail)
            return AddToTail(item);

        item->next = node->next;
        node->next = item;
        return 0;
    }

    void Purge()
    {
        FnTrace("SList::Purge()");
        while (list_head)
        {
            type *tmp = list_head;
            list_head = list_head->next;
            delete tmp;
        }
        list_tail = nullptr;        // REFACTOR: Changed NULL to nullptr for modern C++
    }

    int Remove(type *node)
    {
        FnTrace("SList::Remove()");
        if (node == nullptr)        // REFACTOR: Changed NULL to nullptr for modern C++
            return 1;
        for (type *n = list_head, *p = nullptr; n != nullptr; p = n, n = n->next)  // REFACTOR: Changed NULL to nullptr for modern C++
            if (node == n)
            {
                if (p)
                    p->next = node->next;
                else
                    list_head = node->next;
                if (list_tail == node)
                    list_tail = p;
                node->next = nullptr;   // REFACTOR: Changed NULL to nullptr for modern C++
                return 0;
            }
        return 1;
    }

    int Count()
    {
        FnTrace("SList::Count()");
        int count = 0;
        for (type *n = list_head; n != nullptr; n = n->next)  // REFACTOR: Changed NULL to nullptr for modern C++
            ++count;
        return count;
    }

    type *Index(int i)
    {
        FnTrace("SList::Index()");
        if (i < 0)
            return nullptr;         // REFACTOR: Changed NULL to nullptr for modern C++

        type *n = list_head;
        while (n != nullptr && i > 0)  // REFACTOR: Changed NULL to nullptr for modern C++
            --i, n = n->next;
        return n;
    }

    // Operators
    type *operator[](int i) { return Index(i); }
};

template <class type>
class DList
{
    type *list_head, *list_tail;
    type *InternalSort(type *list, int (*cmp)(type *, type *))
    {
        FnTrace("DList::InternalSort()");
        type *p, *q, *e, *tail;
        int insize, nmerges, psize, qsize, i;
        
        if (list == nullptr)        // REFACTOR: Changed NULL to nullptr for modern C++
            return nullptr;          // REFACTOR: Changed NULL to nullptr for modern C++

        insize = 1;
        
        while (1)
        {
            p = list;
            list = nullptr;          // REFACTOR: Changed NULL to nullptr for modern C++
            tail = nullptr;          // REFACTOR: Changed NULL to nullptr for modern C++
            
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
                    if (!q)
                        break;
                }
                
                /* if q hasn't fallen off end, we have two lists to merge */
                qsize = insize;
                
                /* now we have two lists; merge them */
                while (psize > 0 || (qsize > 0 && q))
                {
                    /* decide whether next element of merge comes from p or q */
                    if (psize == 0)
                    {
                        /* p is empty; e must come from q. */
                        e = q; q = q->next; qsize--;
                    }
                    else if (qsize == 0 || !q)
                    {
                        /* q is empty; e must come from p. */
                        e = p; p = p->next; psize--;
                    }
                    else if (cmp(p,q) <= 0)
                    {
                        /* First element of p is lower (or same);
                         * e must come from p. */
                        e = p; p = p->next; psize--;
                    }
                    else
                    {
                        /* First element of q is lower; e must come from q. */
                        e = q; q = q->next; qsize--;
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
            tail->next = nullptr;    // REFACTOR: Changed NULL to nullptr for modern C++
            
            /* If we have done only one merge, we're finished. */
            if (nmerges <= 1)   /* allow for nmerges==0, the empty list case */
                return list;
            
            /* Otherwise repeat, merging lists twice the size */
            insize *= 2;
        }
    }
    
public:
    // Constructors
    DList()           { list_head = nullptr; list_tail = nullptr; }  // REFACTOR: Changed NULL to nullptr for modern C++
    DList(type *item) { list_head = item; list_tail = item; }

    // Destructor
    ~DList()
    {
        FnTrace("DList::~DList()");
        Purge();
    }

    // Member Functions
    type *Head() { return list_head; }
    type *Tail() { return list_tail; }

    int IsEmpty()
    {
        return (list_head == nullptr);  // REFACTOR: Changed NULL to nullptr for modern C++
    }

    int AddToHead(type *item)
    {
        FnTrace("DList::AddToHead()");
        if (item == nullptr)            // REFACTOR: Changed NULL to nullptr for modern C++
            return 1;

        item->fore = nullptr;           // REFACTOR: Changed NULL to nullptr for modern C++
        item->next = list_head;
        if (list_head)
            list_head->fore = item;
        else
            list_tail = item;
        list_head = item;
        return 0;
    }

    int AddToTail(type *item)
    {
        FnTrace("DList::AddToTail()");
        if (item == nullptr)        // REFACTOR: Changed NULL to nullptr for modern C++
            return 1;

        item->fore = list_tail;
        item->next = nullptr;       // REFACTOR: Changed NULL to nullptr for modern C++
        if (list_tail)
            list_tail->next = item;
        else
            list_head = item;
        list_tail = item;
        return 0;
    }

    int AddAfterNode(type *node, type *item)
    {
        FnTrace("DList::AddAfterNode()");
        if (node == nullptr)        // REFACTOR: Changed NULL to nullptr for modern C++
            return AddToHead(item);
        if (node == list_tail)
            return AddToTail(item);

        item->fore = node;
        item->next = node->next;
        node->next->fore = item;
        node->next = item;
        return 0;
    }

    int AddBeforeNode(type *node, type *item)
    {
        FnTrace("DList::AddBeforeNode()");
        if (node == nullptr)        // REFACTOR: Changed NULL to nullptr for modern C++
            return AddToTail(item);
        if (node == list_head)
            return AddToHead(item);

        item->next = node;
        item->fore = node->fore;
        item->fore->next = item;
        node->fore = item;
        return 0;
    }

    int Exists(type *item, int (*cmp)(type *, type*))
    {
        FnTrace("DList::Exists()");
        if (item == nullptr)        // REFACTOR: Changed NULL to nullptr for modern C++
            return 1;

        type *curr = list_head;
        int found = 0;

        while (curr != nullptr && found == 0)  // REFACTOR: Changed NULL to nullptr for modern C++
        {
            if (cmp(item, curr) == 0)
                found = 1;
            else
                curr = curr->next;
        }

        return found;
    }

    int Remove(type *item)
    {
        FnTrace("DList::Remove()");
        if (item == nullptr)        // REFACTOR: Changed NULL to nullptr for modern C++
            return 1;

        if (list_head == item)
            list_head = item->next;
        if (list_tail == item)
            list_tail = item->fore;
        if (item->next)
            item->next->fore = item->fore;
        if (item->fore)
            item->fore->next = item->next;
        item->fore = nullptr;       // REFACTOR: Changed NULL to nullptr for modern C++
        item->next = nullptr;       // REFACTOR: Changed NULL to nullptr for modern C++
        return 0;
    }

    int RemoveSafe(type *node)
    {
        FnTrace("DList::RemoveSafe()");
        if (node == nullptr)        // REFACTOR: Changed NULL to nullptr for modern C++
            return 1;
        for (type *n = list_head; n != nullptr; n = n->next)  // REFACTOR: Changed NULL to nullptr for modern C++
            if (n == node)
                return Remove(node);
        return 1;
    }

    void Purge()
    {
        FnTrace("DList::Purge()");

        while (list_head)
        {
            type *tmp = list_head;
            list_head = list_head->next;
            delete tmp;
        }
        list_tail = nullptr;        // REFACTOR: Changed NULL to nullptr for modern C++
    }

    int Count() const
    {
        FnTrace("DList::Count()");
        int count = 0;
        for (type *n = list_head; n != nullptr; n = n->next)  // REFACTOR: Changed NULL to nullptr for modern C++
            ++count;
        return count;
    }

    type *Index(int i)
    {
        FnTrace("DList::Index()");
        if (i < 0)
            return nullptr;         // REFACTOR: Changed NULL to nullptr for modern C++

        type *n = list_head;
        while (n != nullptr && i > 0)  // REFACTOR: Changed NULL to nullptr for modern C++
        {
            --i;
            n = n->next;
        }
        return n;
    }

    int Sort(int (*cmp)(type *, type *))
    {
        FnTrace("DList::Sort()");
        list_head = InternalSort(list_head, cmp);
        list_tail = list_head;
        if (list_tail != nullptr)          // REFACTOR: Changed NULL to nullptr for modern C++
        {
            while (list_tail->next != nullptr)  // REFACTOR: Changed NULL to nullptr for modern C++
                list_tail = list_tail->next;
        }
        return 0;
    }

    // Operators
    type *operator[](int i) { return Index(i); }
};
