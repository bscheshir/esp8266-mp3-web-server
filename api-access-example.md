# Проигрывание уведомления.

Команды `API`, которые будет использовать `esp8266` при опросе сервера уведомлений.

- `GET` `/notice` - Получить последнее неотработавшее уведомление.

- `POST` `/notice/{id}` - Отметить уведомление как отработавшее.


## Концепция

Сервер уведомлений переодически создаёт уведомление с некоторым сроком жизни, содержащее идентификатор и номер уведомления для проигрывания. 

Клиент с некоторой переодичностью опрашивает сервер на наличие непроигранных уведомлений.

В случае, если таковые имеются - сервер отдаёт одно из них соответственно алгоритмам куча или очередь (в зависимости от настроек) 
 
Клиент получает эти данные в `json` формате в виде массива 
```php
['id' => 1, 'songId' => 1]
```

После окончания проигрывания уведомления клиент отправляет новый запрос `API`, указывающий что данное уведомление отработало. 

## Реализация на основании `bscheshirwork/docker-yii2-app-advanced-redis`

[https://github.com/bscheshirwork/docker-yii2-app-advanced-redis](https://github.com/bscheshirwork/docker-yii2-app-advanced-redis)

1. Создать новую ветку либо в существующую добавить 
```sh
git pull bscheshirwork/docker-yii2-app-advanced-rbac api
```

2. Создать необходимые сертификаты, действуя согласно [инструкции](https://github.com/bscheshirwork/docker-yii2-app-advanced-rbac/blob/api/docs/about-api-ssl.md)

3. Создать контроллеры, модели (версированные версии)
`api/common/controllers/NoticeController.php`
```php
<?php

namespace api\common\controllers;


use common\models\Notice;
use yii\rest\ActiveController;
use yii\web\HttpException;

class NoticeController extends ActiveController
{
    public $modelClass = '\api\common\models\Notice';

    /**
     * @inheritdoc
     */
    public function actions()
    {
        return [
            'options' => [
                'class' => \yii\rest\OptionsAction::class,
                'collectionOptions' => ['GET', 'OPTIONS'],
                'resourceOptions' => ['GET', 'OPTIONS'],
            ],
        ];
    }

    /**
     * @inheritdoc
     */
    protected function verbs()
    {
        return [
            'last' => ['GET'],
            'delete' => ['POST'],
        ];
    }

    /**
     * Get last item of notifications
     * @throws HttpException
     * @throws \yii\base\InvalidConfigException
     */
    public function actionLast()
    {
        /** @var Notice $modelClass */
        $modelClass = $this->modelClass;
        if ($model = $modelClass::findLast()) {
            return $model;
        }

        throw new HttpException(404, "404 NotFound");
    }

    /**
     * Get first item of notifications
     * @throws HttpException
     * @throws \yii\base\InvalidConfigException
     */
    public function actionFirst()
    {
        /** @var Notice $modelClass */
        $modelClass = $this->modelClass;
        if ($model = $modelClass::findFirst()) {
            return $model;
        }

        throw new HttpException(404, "404 NotFound");
    }

    /**
     * @param $id
     * @return null
     * @throws HttpException
     * @throws \yii\base\InvalidConfigException
     */
    public function actionDelete($id)
    {
        /** @var Notice $modelClass */
        $modelClass = $this->modelClass;
        if ($modelClass::deleteOne($id)) {
            \Yii::$app->response->statusCode = 204;

            return null;
        }

        throw new HttpException(404, "404 NotFound");
    }
}
```
`api/modules/v1/controllers/NoticeController.php`
```php
<?php

namespace api\v1\controllers;


class NoticeController extends \api\common\controllers\NoticeController
{
}
```
`common/models/Notice.php`
```php
<?php

namespace common\models;


use yii\base\Model;
use yii\helpers\Json;

/**
 * @property \yii\redis\Connection redis
 */
class Notice extends Model
{
    /**
     * 4% chance to run garbage collector on delete action
     */
    const GC_CHANCE = 4;

    public $id;
    public $songId;

    /**
     * Common key prefix for all corresponded data in redis
     * self::class - share prefix for console, web, api and other children
     * static::class - separate by children
     * @return string
     */
    protected function keyPrefix()
    {
        return self::class;
    }

    /**
     * Key for timestamp query in redis
     * @return string
     */
    protected function zKey()
    {
        return $this->keyPrefix() . '::z';
    }

    /**
     * Key for autoincrement value in redis
     * @return string
     */
    protected function iKey()
    {
        return $this->keyPrefix() . '::i';
    }

    /**
     * Key for concrete model data in redis
     * @param $id
     * @return string
     */
    protected function keyModel($id)
    {
        return $this->keyPrefix() . '::' . $id;
    }

    /**
     * Load data from the storage by id.
     * @param string $key redis key for data
     * @return bool|self
     */
    protected function loadByKey($key)
    {
        if (!($raw = $this->redis->get($key))) {
            return false;
        }
        $data = Json::decode($raw);
        $this->setAttributes($data, false);

        return $this;
    }

    /**
     * Get `redis` component. Can be use as `$this->redis`
     * @return null|object
     */
    public function getRedis()
    {
        return \Yii::$app->get('redis');
    }

    /**
     * @return int the number of seconds after which data will be seen as 'garbage' and cleaned up.
     * The default value is 1440 seconds (or the value of "session.gc_maxlifetime" set in php.ini).
     */
    public function getTimeout()
    {
        return (int) ini_get('session.gc_maxlifetime');
    }

    /**
     * Get last value of autoincrement.
     * @return int|mixed
     */
    public function getLastId()
    {
        try {
            return $this->redis->get($this->iKey());
        } catch (\Exception $exception) {
            return 0;
        }
    }

    /**
     * @inheritDoc
     */
    public function rules()
    {
        return [
            [
                ['songId'],
                'required',
            ],
            [
                ['songId'],
                'exist',
                'skipOnError' => true,
                'targetClass' => RelatedModel::className(),
                'targetAttribute' => ['relatedModelId' => 'id'],
                'filter' => function (RelatedModelQuery $query) {
                    $query->active();
                },
            ],
        ];
    }

    /**
     * Save the model data to storage
     * @param bool $validate
     * @return bool
     */
    public function save($validate = true)
    {
        if ($validate) {
            if (!$this->validate()) {
                return false;
            }
        }
        if (empty($this->id)) {
            $this->redis->watch($this->iKey());
            $lastId = $this->redis->get($this->iKey());
            $this->id = (int) $lastId + 1;
        }
        $key = $this->keyModel($this->id);
        $data = Json::encode($this->attributes);
        $this->redis->multi();
        $this->redis->zadd($this->zKey(), 'CH', time(), $key);
        $this->redis->set($key, $data);
        $this->redis->set($this->iKey(), max($this->id, $this->redis->get($this->iKey())));
        $exec = $this->redis->exec();
        $result = !empty($exec[0]) && !empty($exec[1]);

        return $result;
    }

    /**
     * Delete the model data from storage
     * @return bool
     */
    public function delete()
    {
        $id = $this->keyModel($this->id);
        $this->redis->multi();
        $this->redis->zrem($this->zKey(), $id);
        $this->redis->del($id);
        $this->redis->exec();

        return true;
    }

    /**
     * Destroy all expired
     */
    public function removeExpired()
    {
        $this->redis->multi();
        $this->redis->zrangebyscore($this->zKey(), '-inf', time() - $this->getTimeout());
        $this->redis->zremrangebyscore($this->zKey(), '-inf', time() - $this->getTimeout());
        $exec = $this->redis->exec();
        if ($exec[0]) {
            $this->redis->del(...$exec[0]);
        }

        return true;
    }

    /**
     * Insert new data set into storage
     * @param array $data
     * @return bool
     * @throws \yii\base\InvalidConfigException
     */
    public static function put(array $data)
    {
        /** @var self $model */
        $model = \Yii::createObject(self::class);
        $model->load($data, '');

        return $model->save();
    }

    /**
     * Remove data from storage
     * @param int $id
     * @return bool
     * @throws \yii\base\InvalidConfigException
     */
    public static function deleteOne(int $id)
    {
        if (!($id = (string) $id)) {
            return false;
        }
        /** @var self $model */
        $model = \Yii::createObject(self::class);
        $model->id = $id;
        if (mt_rand(0, 100) <= self::GC_CHANCE) {
            $model->removeExpired();
        }

        return $model->delete();
    }

    /**
     * Load concrete element from storage
     * @param int $id
     * @return bool|object
     * @throws \yii\base\InvalidConfigException
     */
    public static function findOne(int $id)
    {
        if (!($id = (string) $id)) {
            return false;
        }
        /** @var self $model */
        $model = \Yii::createObject(self::class);

        return $model->loadByKey($model->keyModel($id));
    }

    /**
     * Load first model from storage (query)
     * @return bool|self
     * @throws \yii\base\InvalidConfigException
     */
    public static function findFirst()
    {
        /** @var self $model */
        $model = \Yii::createObject(self::class);
        if (!($set = $model->redis->zrangebyscore($model->zKey(), time() - $model->getTimeout(), '+inf'))) {
            return false;
        }
        if (!is_array($set)) {
            return false;
        }
        $key = array_shift($set);

        return $model->loadByKey($key);
    }

    /**
     * Load last model from storage (stack)
     * @return bool|self
     * @throws \yii\base\InvalidConfigException
     */
    public static function findLast()
    {
        /** @var self $model */
        $model = \Yii::createObject(self::class);
        if (!($set = $model->redis->zrevrangebyscore($model->zKey(), time(), time() - $model->getTimeout()))) {
            return false;
        }
        if (!is_array($set)) {
            return false;
        }
        $key = array_shift($set);

        return $model->loadByKey($key);
    }
}

```
`api/common/models/Notice.php`
```php
<?php

namespace api\common\models;


class Notice extends \common\models\Notice
{
}
```



4. Добавить в настройки `API` (* yii\rest\GroupUrlRule - PR ещё не принят)
```php
        'urlManager' => [
            'enablePrettyUrl' => true,
            'showScriptName' => false,
            'rules' => [
                [
                    'class' => 'yii\rest\GroupUrlRule',
                    'prefix' => 'v1',
                    'rules' => [
                        [
                            'controller' => 'feedback',
                            'pluralize' => false,
                            'only' => ['create', 'options'],
                        ],
                        [
                            'controller' => ['notice'],
                            'pluralize' => false,
                            'extraPatterns' => [
                                'GET' => 'last',
                                'POST {id}' => 'delete',
                            ],
                            'only' => ['last', 'delete'],
                        ],
                    ],
                ],
            ],
        ],

```
