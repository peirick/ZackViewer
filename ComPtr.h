#pragma once

// A wrapper around a pointer to a COM object that does a Release on
// descruction. T should be a subclass of IUnknown.
template<typename T>
class ComPtr {
public:
	ComPtr() :ptr_(nullptr) { }
	// ptr should be already AddRef'ed for this reference.
	ComPtr(T* ptr) :ptr_(ptr) { }
	~ComPtr() { if (ptr_) ptr_->Release(); }

	T* get() { return ptr_; }
	T* new_ref() { ptr_->AddRef(); return ptr_; }
	// new_ptr should be already AddRef'ed for this new reference.
	void reset(T* new_ptr) { if (ptr_ != nullptr) ptr_->Release(); ptr_ = new_ptr; }
	// Allows to pass the the pointer as an 'out' parameter. If a non-NULL value
	// is written to it, it should be a valid pointer and already AddRef'ed for
	// this new reference.
	T** get_out_storage() { reset(nullptr); return &ptr_; }
	T* operator->() { return ptr_; }
	T& operator*() { return *ptr_; }
private:
	T* ptr_;

	// No copy and assign.
	ComPtr(const ComPtr&) = delete;
	void operator=(const ComPtr&) = delete;
};
