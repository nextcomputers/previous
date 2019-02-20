#ifndef _FILETABLE_H_
#define _FILETABLE_H_

#define TABLE_SIZE 1024

#include "tree.h"
#include "compat.h"

typedef struct {
    char*        path;
    size_t       nPathLen;
    FILE_HANDLE* handle;
    bool         bCached;
} FILE_ITEM;

typedef struct _FILE_TABLE {
	 tree_node_<FILE_ITEM>* pItems[TABLE_SIZE];
    _FILE_TABLE*            pNext;
} FILE_TABLE;

typedef struct _CACHE_LIST {
    FILE_ITEM   *pItem;
    _CACHE_LIST *pNext;
} CACHE_LIST;

class CFileTable
{
public:
    CFileTable();
    ~CFileTable();
    uint64_t               GetIDByPath(const char *path);
    FILE_HANDLE*           GetHandleByPath(const char *path);
    bool                   GetPathByHandle(FILE_HANDLE *handle, std::string &path);
	tree_node_<FILE_ITEM>* FindItemByPath(const char *path);
    bool                   RemoveItem(const char *path);
	void                   RenameFile(const char *pathFrom, const char *pathTo);

protected:
    tree_node_<FILE_ITEM>* AddItem(const char *path);

private:
    FILE_TABLE* m_pFirstTable;
    FILE_TABLE* m_pLastTable;
    size_t      m_nTableSize;
    CACHE_LIST* m_pCacheList;

	tree_node_<FILE_ITEM>* GetItemByID(uint64_t nID);
    void PutItemInCache(FILE_ITEM *pItem);
};
#endif
