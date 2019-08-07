/*
 * DataSourceInterface.h
 *
 *  Created on: 12-Oct-2014
 *      Author: sumeetc
 */

#ifndef DATASOURCEINTERFACE_H_
#define DATASOURCEINTERFACE_H_
#include <iostream>
#include "Timer.h"
#include "DateFormat.h"
#include "LoggerFactory.h"
#include "Query.h"
#include "QueryBuilder.h"
#include "RegexUtil.h"
#include "DataSourceMapping.h"
#include "Reflector.h"
#include "BinaryData.h"
#include "Query.h"
#include "ConnectionPooler.h"
#include "IDGenerator.h"

class DataSourceInterface : public IDGenerator {
	friend class DataSourceManager;
protected:
	ConnectionPooler* pool;
	Mapping* mapping;
	std::string appName;
	Reflector* reflector;
	void* context;
	bool executeInsertInternal(Query& query, void* entity);
	virtual bool executeInsert(Query& query, void* entity)=0;
	virtual bool isGetDbEntityForBulkInsert()=0;
	virtual void* getDbEntityForBulkInsert(void* entity, const std::string& clasName, std::string& error)=0;
	virtual bool executeInsertBulk(Query& query, std::vector<void*> entities, std::vector<void*> dbEntities)=0;
	virtual bool executeUpdateBulk(Query& query, std::vector<void*> entities, std::vector<void*> dbEntities)=0;
	virtual bool executeUpdate(Query& query, void* entity)=0;
	virtual bool remove(const std::string& clasName, GenericObject& id)=0;

	virtual void* executeQuery(Query& query, const bool& isObj)=0;
	virtual void* executeQuery(QueryBuilder& qb, const bool& isObj)=0;
	virtual long getNumRows(const std::string& clasName)=0;
	virtual void empty(const std::string& clasName)=0;

	virtual void* getContext(void*)=0;
	virtual void destroyContext(void*)=0;

	void assignId(DataSourceEntityMapping& dsemp, ClassInfo* clas, void* entity);
public:
	static std::string BLANK;
	DataSourceInterface();
	virtual ~DataSourceInterface();
	virtual bool startTransaction()=0;
	virtual bool commit()=0;
	virtual bool rollback()=0;
	virtual void procedureCall(const std::string&)=0;
	virtual std::vector<std::map<std::string, GenericObject> > execute(Query& query)=0;
	virtual bool executeUpdate(Query& query)=0;
	virtual std::vector<std::map<std::string, GenericObject> > execute(QueryBuilder& qb)=0;

	bool startSession(void*);
	bool startSession();
 	bool endSession();

	template<class T> bool insert(T& t);
	template<class T> bool update(T& t);
	template<class T> std::map<int, std::string> bulkUpdate(std::vector<T>& vecT);
	template<class T> bool remove(GenericObject& id);
	template<class T> void empty();
	template<class T> long getNumRows();
	template<class T> std::map<int, std::string> bulkInsert(std::vector<T>& vecT);
	template<class T> T get(GenericObject& id);
	template<class T> std::vector<T> getAll();

	template<class T> std::vector<T> getList(Query& query);
	template<class T> T get(Query& query);

	template<class T> std::vector<T> getList(QueryBuilder& qb);
	template<class T> T get(QueryBuilder& qb);
};

template<class T>
inline bool DataSourceInterface::insert(T& t) {
	std::string clasName = CastUtil::getClassName(t);
	Query q(BLANK, clasName);
	return executeInsertInternal(q, &t);
}

template<class T>
inline bool DataSourceInterface::update(T& t) {
	std::string clasName = CastUtil::getClassName(t);
	Query q(BLANK, clasName);
	return executeUpdate(q, &t);
}

template<class T>
inline std::map<int, std::string> DataSourceInterface::bulkInsert(std::vector<T>& vecT) {
	T t;
	std::string clasName = CastUtil::getClassName(t);
	DataSourceEntityMapping dsemp = mapping->getDataSourceEntityMapping(clasName);
	ClassInfo* clas = reflector->getClassInfo(clasName, appName);
	std::vector<void*> dbEntities;
	std::vector<void*> entities;
	std::map<int, std::string> errors;

	for (unsigned int k = 0; k < vecT.size(); k++) {
		T* t = &(vecT.at(k));
		if(dsemp.isIdGenerate() && dsemp.getIdgendbEntityType()!="identity") {
			assignId(dsemp, clas, t);
		}
		std::string error;
		if(isGetDbEntityForBulkInsert())
		{
			void* dbEntity = getDbEntityForBulkInsert(t, clasName, error);
			if(dbEntity!=NULL) {
				entities.push_back(&(vecT.at(k)));
				dbEntities.push_back(dbEntity);
			} else if(error!="") {
				errors.insert(std::pair<int, std::string>(k, error));
			}
		}
		else
		{
			entities.push_back(&(vecT.at(k)));
		}
	}
	if(entities.size()>0)
	{
		Query q(BLANK, clasName);
		executeInsertBulk(q, entities, dbEntities);
	}
	return errors;
}

template<class T>
inline std::map<int, std::string> DataSourceInterface::bulkUpdate(std::vector<T>& vecT) {
	T t;
	std::string clasName = CastUtil::getClassName(t);
	//ClassInfo* clas = reflector->getClassInfo(clasName, appName);
	std::vector<void*> dbEntities;
	std::vector<void*> entities;
	std::map<int, std::string> errors;
	for (unsigned int k = 0; k < vecT.size(); k++) {
		T* t = &(vecT.at(k));
		std::string error;
		if(isGetDbEntityForBulkInsert())
		{
			void* dbEntity = getDbEntityForBulkInsert(t, clasName, error);
			if(dbEntity!=NULL) {
				entities.push_back(&(vecT.at(k)));
				dbEntities.push_back(dbEntity);
			} else if(error!="") {
				errors.insert(std::pair<int, std::string>(k, error));
			}
		}
		else
		{
			entities.push_back(&(vecT.at(k)));
		}
	}
	if(entities.size()>0)
	{
		Query q(BLANK, clasName);
		executeUpdateBulk(q, entities, dbEntities);
	}
	return errors;
}

template<class T>
inline T DataSourceInterface::get(GenericObject& id) {
	T t;
	std::string clasName = CastUtil::getClassName(t);
	DataSourceEntityMapping& dsemp = mapping->getDataSourceEntityMapping(clasName);
	std::string idColName = dsemp.getColumnForProperty(dsemp.getIdPropertyName());
	QueryBuilder qb;
	qb.fromClass(clasName, BLANK).condition().where(idColName, QueryOperator::EQUALS, id).end();
	std::vector<std::map<std::string, GenericObject> > results;
	void* vect = executeQuery(qb, true);
	std::vector<T> vecT;
	if (vect != NULL) {
		vecT = *(std::vector<T>*) vect;
		delete (std::vector<T>*)vect;
		if(vecT.size()>0)
		{
			t = vecT.at(0);
		}
	}
	return t;
}

template<class T>
inline std::vector<T> DataSourceInterface::getAll() {
	T t;
	std::string clasName = CastUtil::getClassName(t);
	Query query(BLANK, clasName);
	std::vector<T> vecT;
	void* vect = executeQuery(query, true);
	if (vect != NULL) {
		vecT = *(std::vector<T>*) vect;
		delete (std::vector<T>*)vect;
	}
	return vecT;
}

template<class T>
inline std::vector<T> DataSourceInterface::getList(Query& query) {
	T t;
	std::vector<T> vecT;
	std::string clasName = CastUtil::getClassName(t);
	if(query.getClassName()!=clasName)return vecT;
	void* vect = executeQuery(query, true);
	if (vect != NULL) {
		vecT = *(std::vector<T>*) vect;
		delete (std::vector<T>*)vect;
	}
	return vecT;
}

template<class T>
inline T DataSourceInterface::get(Query& query) {
	T t;
	std::string clasName = CastUtil::getClassName(t);
	if(query.getClassName()!=clasName)return t;
	void* vect = executeQuery(query, true);
	std::vector<T> vecT;
	if (vect != NULL) {
		vecT = *(std::vector<T>*) vect;
		delete (std::vector<T>*)vect;
		if(vecT.size()>0)
		{
			t = vecT.at(0);
		}
	}
	return t;
}

template<class T>
inline std::vector<T> DataSourceInterface::getList(QueryBuilder& qb) {
	T t;
	std::vector<T> vecT;
	std::string clasName = CastUtil::getClassName(t);
	if(qb.getClassName()!=clasName)return vecT;
	void* vect = executeQuery(qb, true);
	if (vect != NULL) {
		vecT = *(std::vector<T>*) vect;
		delete (std::vector<T>*)vect;
	}
	return vecT;
}

template<class T>
inline T DataSourceInterface::get(QueryBuilder& qb) {
	T t;
	std::string clasName = CastUtil::getClassName(t);
	if(qb.getClassName()!=clasName)return t;
	void* vect = executeQuery(qb, true);
	std::vector<T> vecT;
	if (vect != NULL) {
		vecT = *(std::vector<T>*) vect;
		delete (std::vector<T>*)vect;
		if(vecT.size()>0)
		{
			t = vecT.at(0);
		}
	}
	return t;
}

template<class T>
inline void DataSourceInterface::empty() {
	T t;
	std::string clasName = CastUtil::getClassName(t);
	return empty(clasName);
}

template<class T>
inline long DataSourceInterface::getNumRows() {
	T t;
	std::string clasName = CastUtil::getClassName(t);
	return getNumRows(clasName);
}

template<class T>
inline bool DataSourceInterface::remove(GenericObject& id) {
	T t;
	std::string clasName = CastUtil::getClassName(t);
	return remove(clasName, id);
}

#endif /* DATASOURCEINTERFACE_H_ */
