#ifndef BC_PTR_H_
#define BC_PTR_H_

#include<string>
#include<cstddef>
#include<mutex>

/** 
* @brief Borrow checker error class
* * This is the bc_ptr related errors handler
*/
class bc_error
{
    private:
        /**
         * @brief Stored error message
         */
        std::string message;
    public:
        /**
         * @brief bc_error constructor
         * @param msg The message associated with the error
         */
        bc_error(const std::string& msg): message(msg) 
        {

        }
        /**
         * @brief standard way of returning the associated error message
         */
        [[nodiscard]]
        std::string what() const noexcept
        {
            return message;
        }
};

/**
 * @brief Wrapper for pointers returned by at(), operator[], * or -> 
 * Used to throw when an attempt to modify a reference is made
 */
template<class T>
class ptr_wrapper
{
    private:
        /**
         * @brief Marks the usage of a pointer (mutable/immutable)
          */
        const bool is_ref [[maybe_unused]];
        /**
         * @brief Underlying pointer (what the class wraps)
         */
        T* ul_ptr [[maybe_unused]];
    public:
        /**
         * @brief Wrapper constructor
         * @param ptr Pointer to be returned when a conversion or equal operator is called
         * @param is_ref When set to true, attempts to change the underlying pointer's value throws
          */
        ptr_wrapper(T* ptr,bool _is_ref=0) : is_ref(_is_ref),ul_ptr(ptr)
        {
            
        }
        /**
         * @brief Conversion operator
         * @return Underlying pointer's value
         */
        [[nodiscard]]
        operator T()
        {
            if(ul_ptr==nullptr)
                throw bc_error("error: attempted to unwrap a null pointer");
            return *ul_ptr;
        }
        /**
         * @brief Equals operator
         * @param x Value that will be assigned to the pointer (if it is not marked as a reference)
         * @throw bc_error if it is attempted to modify a pointer's value (if it is marked as a reference)
         */
        void operator=(T x)
        {
            if(ul_ptr==nullptr)
                throw bc_error("error: attempted to unwrap a null pointer");
            if(is_ref)
                throw bc_error("error: cannot assign a value to a reference or immutable object");
            *ul_ptr=x;
        }
};

/**
 * @brief Borrow Checked pointer class
 */
template<class T>
class bc_ptr
{
    private:
        /**
         * @param mtx used to avoid data races and ensure thread safety
         */
        mutable std::mutex mtx; 
        /**
         * @param ul_ptr Underlying pointer, wrapped in the bc_ptr class
         */
        T* ul_ptr;
        /**
         * @param references_ptr Stores a pointer of the bc_ptr this pointer references or borrows
         */
        bc_ptr* references_ptr;
        /**
         * @param bound Decides the size of the allocated memory block for this pointer
         * @param activereferences Stores how many external references are made to this object's
         * underlying pointer
         */
        std::size_t bound,activereferences;
        /**
         * @param assigned Keeps track of memory allocation for this pointer
         * (false means no allocation has been done, i.e. the pointer is empty or a reference)
         * (true means the underlying pointer is storing the address of an allocated block of memory)
         * @param borrowing True if this pointer is borrowing a different pointer, false otherwise
         * @param borrowed True if this pointer is being borrowed, false otherwise
         */
        bool assigned,borrowing,borrowed;
        /**
         * @brief Perform validation of a operation that modifies the value of the underlying pointer
         * @throw bc_error if the operation is invalid (i.e. it would cause side effects)
         */
        void validate_ptr_modif()
        {
            mtx.lock();
            if(references_ptr!=nullptr and !borrowing)
                throw bc_error("error: cannot modify a reference");
            if(!assigned)
                throw bc_error("error: cannot modify an unassigned pointer");
            mtx.unlock();
        }
        /**
         * @brief Retrieve a pointer to a bc_ptr object or its underlying pointer's owner
         * @return Pointer to requested bc_ptr object
         * @throw bc_error if the request is invalid or breaks borrow checking rules
         */
        bc_ptr<T>* get_bc_ptr_ref()
        {
            mtx.lock();
            if(!bound)
                throw bc_error("error: cannot reference an empty pointer");
            if(borrowed or borrowing)
                throw bc_error("error: cannot reference an active borrow");    
            if(references_ptr!=nullptr)
            {
                bc_ptr<T>* ref_ptr = references_ptr;
                mtx.unlock();
                return ref_ptr;
            }
            bc_ptr<T>* this_ptr = this;
            mtx.unlock();
            return this_ptr;
        }
        /**
         * @brief Validates borrow operations
         * @throw bc_error if request is invalid
         */
        void validate_borrow()
        {
            mtx.lock();
            if(!assigned and references_ptr!=nullptr)
                throw bc_error("error: cannot borrow a reference");
            if(!bound)
                throw bc_error("error: cannot borrow an unassigned pointer");
            if(borrowed or borrowing)
                throw bc_error("error: attempted multiple borrow");
            mtx.unlock();
        }
        /**
         * @brief Internal utility for assigning blocks of memory to pointers
         */
        void assign(std::size_t size)
        {
            mtx.lock();
            ul_ptr = new T[size];
            assigned = 1;
            bound = size;
            activereferences = 0;
            borrowing = 0;
            borrowed = 0;
            references_ptr = nullptr;
            mtx.unlock();
        }
        /**
         * @brief Internal utility used for unassigning blocks of memory attributed
         * to pointers. Is also called when destruction of an object occurs
         * @throw bc_error when unassignment conditions are not met
         * (i.e unassignment would cause side effects)
         */
        void unassign()
        {
            mtx.lock();
            if(borrowed)
                throw bc_error("error: cannot unassign a borrowed pointer");
            if(activereferences)
                throw bc_error("error: cannot unassign a referenced pointer");
         
            if(borrowing)
            {
                references_ptr->borrowed=0;
                ul_ptr = nullptr;
                assigned = 0;
                bound = 0;
                borrowing = 0;
                references_ptr=nullptr;
                mtx.unlock();
                return;
            }
            if(references_ptr!=nullptr)
            {
                references_ptr->activereferences--;
                ul_ptr = nullptr;
                bound = 0;
                references_ptr=nullptr;
                mtx.unlock();
                return;
            }
            if(!assigned)
            {
                mtx.unlock();
                return;
            }
            assigned = 0;
            bound = 0;
            delete[] ul_ptr;
            ul_ptr=nullptr;
            mtx.unlock();
        }
    public:
        /**
         * @brief Default constructor. Assigns a block of memory of size 1
         */
        bc_ptr()
        {
            assign(1);
        }
        /**
         * @brief Absorb constructor. Absorbs an external pointer
         * @param ptr Pointer to be absorbed
         * @param size Size of the given pointer
         */
        bc_ptr(T*& ptr,std::size_t size)
        {
            assigned=0;
            borrowing=0;
            borrowed=0;
            references_ptr=nullptr;
            activereferences=0;
            ul_ptr=nullptr;
            absorb(ptr,size);
        }
        /**
         * @brief Constructor with a size parameter
         * @param size The size of the block of memory to be assigned to the underlying pointer
         */
        bc_ptr(std::size_t size)
        {
            if(!size)
            {
                mtx.lock();
                ul_ptr = nullptr;
                assigned = 0;
                bound = 0;
                borrowed = 0;
                borrowing = 0;
                references_ptr = nullptr;
                activereferences = 0;
                mtx.unlock();
                return;
            }
            assign(size);
        }
        /**
         * @brief Destructor
         * @see unassign
         */
        ~bc_ptr()
        {
            unassign();
        }


        /**
         * @brief Deleted operators
         * They have been deleted to avoid illegal or nonsensical operations
         */
        void operator>(bc_ptr<T> p) = delete;
        void operator=(const bc_ptr<T>& p) = delete;
        void operator=(bc_ptr<T>&& p) = delete;
        bc_ptr(bc_ptr<T>&& p) = delete;
        bc_ptr(const bc_ptr<T>& p) = delete;


        /** 
         * @brief Size getter
         * @return The size of the underlying pointer's allocated block of memory (or 0 if unassigned)
         */
        std::size_t size() const noexcept
        {
            return bound;
        }
        /**
         * @brief Assignment status getter
         * @return True if the underlying pointer is assigned a block of memory, false otherwise
         */
        bool is_assigned() const noexcept
        {
            return assigned;
        }
        /**
         * @brief Empty status getter
         * @return True if the underlying pointer is neither assigned nor bound
         * (if the underlying pointer is a reference, it is not assigned, but has a size)
         */
        bool is_empty() const noexcept
        {
            return !assigned and !bound;
        }
        /**
         * @brief Reference status getter
         * @return True if the underlying pointer represents a reference, false otherwise
         */
        bool is_ref() const noexcept
        {
            return references_ptr!=nullptr and !borrowing;
        }
        /**
         * @brief Referenced status getter
         * @return True if the underlying pointer is currently being referenced by other objects
         */
        bool is_referenced() const noexcept
        {
            return activereferences>0;
        }
        /**
         * @brief Mutable status getter
         * @return True if the object is modifiable without side effects, false otherwise
         */
        bool is_mutable() const noexcept
        {
            return !borrowed and !activereferences;
        }
        /** 
         * @brief Borrowing status getter
         * @return True if the object is borrowing another object's underlying pointer, false otherwise 
         */
        bool is_borrowing() const noexcept
        {
            return borrowing;
        }
        /**
         * @brief Is operator
         * @param p The object to compare the current one to
         * @return True if both the current pointer and the pointer passed as an argument
         * point to the same location in memory, false otherwise
         * @note This operator is used instead of the equality operator, because the latter
         * could cause confusion
         */
        bool is(bc_ptr p) const noexcept
        {
            return ul_ptr==p.ul_ptr;
        }
        /**
         * @brief Reassignment method. Shortcut to unassign and immediately assign a pointer
         * @param size Size of the newly assigned block of memory
         * @see unassign
         * @see assign
         */
        void reassign(std::size_t size)
        {
            unassign();
            assign(size);
        }
        /**
         * @brief Dereferencing operator
         * @return Pointer wrapped in ptr_wrapper to return both standalone pointers
         * and also pointers meant to represent references
         * @see ptr_wrapper
         */
        [[nodiscard]]
        ptr_wrapper<T> constexpr operator*()
        {
            mtx.lock();
            T* ul_ptr_temp = ul_ptr;
            if(is_ref() or borrowed or activereferences)
                {
                    mtx.unlock();
                    return ptr_wrapper<T>(ul_ptr_temp,1);
                }
            mtx.unlock();
            return ptr_wrapper<T>(ul_ptr_temp);
        }
        /**
         * @brief Arrow operator. Same as dereferencing operator
         * @see dereferencing operator
         */
        [[nodiscard]]
        ptr_wrapper<T> constexpr operator->()
        {
            mtx.lock();
            T* ul_ptr_temp = ul_ptr;
            if(is_ref() or borrowed or activereferences)
            {
                mtx.unlock();
                return ptr_wrapper<T>(ul_ptr_temp,1);
            }
            mtx.unlock();
            return ptr_wrapper<T>(ul_ptr_temp);
        }
        /**
         * @brief At operator. Used to return a wrapper to a certain position
         * in the underlying pointer's allocated block of memory
         * @param i Index
         * @throw bc_error if the parameter is out of bounds
         * @return wrapped pointer
         * @see ptr_wrapper
         */
        [[nodiscard]]
        ptr_wrapper<T> constexpr at(std::size_t i)
        {
            mtx.lock();
            if(i>=bound)
                throw bc_error("error: out of bounds");
            T* ul_ptr_temp = ul_ptr;
            if(is_ref() or borrowed or activereferences)
            {
                mtx.unlock();
                return ptr_wrapper<T>(ul_ptr_temp+i,1);
            }
            mtx.unlock();
            return ptr_wrapper<T>(ul_ptr_temp+i);
        }
        /**
         * @brief Indexing operator. Syntactic sugar for the at operator above
         * @see at operator
         */
        [[nodiscard]]
        ptr_wrapper<T> constexpr operator[](std::size_t i)
        {
            return at(i);
        }
        /**
         * @brief Clear method. Syntactic sugar for the unassign private method
         * @see unassign
         */
        void clear()
        {
            unassign();
        }
        /**
         * @brief Referencing method. Make the current object a reference to the one passed
         * as an argument
         * @param p Object to reference
         * @throw bc_error if unassignment of current object fails or the retrieval of the argument's
         * pointer fails
         * @see get_bc_ptr_ref
         * @see unassign
         */
        void ref(bc_ptr<T>& p)
        {
            unassign();
            references_ptr = p.get_bc_ptr_ref();
            mtx.lock();
            p.mtx.unlock();
            references_ptr->activereferences++;
            assigned=0;
            bound=p.bound;
            ul_ptr=p.ul_ptr;
            mtx.unlock();
            p.mtx.unlock();
        }
        /**
         * @brief Cloning method. Copy the object passed as an argument element by element
         * @param p The object that is being cloned
         * @throw bc_error if the current object fails mutability validation
         * @see validate_ptr_modif
         */
        void clone(bc_ptr<T> p)
        {
            validate_ptr_modif();
            mtx.lock();
            p.mtx.lock();
            if(bound<p.bound)
                throw bc_error("error: out of bounds");
            bound=p.bound;
            for(std::size_t i=0;i<bound;i++)
                ul_ptr[i]=p.ul_ptr[i];
            mtx.unlock();
            p.mtx.unlock();
        }
        /**
         * @brief Borrow a pointer from another object
         * @param p Object to be borrowed from
         * @throw bc_error if unassignment of current object fails or the argument
         * fails borrow validation
         * @see validate_borrow
         * @see unassign
         */
        void borrow(bc_ptr<T>& p)
        {
            unassign();
            p.validate_borrow();
            mtx.lock();
            p.mtx.lock();
            borrowing=1;
            p.borrowed=1;
            ul_ptr=p.ul_ptr;
            references_ptr=&p;
            assigned=1;
            bound=p.bound;
            mtx.unlock();
            p.mtx.unlock();
        }
        /**
         * @brief Move operator. Permanently move the underlying pointer of an argument
         * to the current object's underlying pointer, unassigning the argument's pointer in the process
         * @param p The object that is being moved
         * @throw bc_error if unassignment fails or the object cannot be moved without side effects
         * @see unassign
         */
        void move(bc_ptr<T>& p)
        {
            unassign();
            mtx.lock();
            p.mtx.lock();
            if(p.borrowed or p.borrowing or p.activereferences or !p.assigned)
                throw bc_error("error: cannot move a borrowed, referenced or unassigned pointer");
            ul_ptr=p.ul_ptr;
            assigned=1;
            bound=p.bound;
            p.assigned=0;
            p.bound=0;
            p.ul_ptr=nullptr;
            mtx.unlock();
            p.mtx.unlock();
        }
        /**
         * @brief Absorb an external pointer with a given size. Assign the external pointer
         * and size to the current object and replace the external pointer with nullptr
         * to avoid modifications to the pointer from outside of the object's constraints and mechanics 
         * @param ptr External pointer
         * @param ptr_size Size of external pointer
         */
        void absorb(T*& ptr,std::size_t ptr_size=1)
        {
            unassign();
            mtx.lock();
            ul_ptr=ptr;
            bound=ptr_size;
            assigned=1;
            ptr=nullptr;
            mtx.unlock();
        }
};
#endif