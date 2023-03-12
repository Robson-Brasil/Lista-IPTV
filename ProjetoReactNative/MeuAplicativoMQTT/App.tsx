import React, { useEffect } from 'react';
import { Text, View } from 'react-native';
import mqtt from 'mqtt';

const App = () => {
  useEffect(() => {
    const client = mqtt.connect('mqtt://test.mosquitto.org');

    client.on('connect', () => {
      console.log('Connected');
      client.subscribe('my/topic');
      client.publish('my/topic', 'Hello, World!');
    });

    client.on('message', (topic, message) => {
      console.log(`Received message on ${topic}: ${message.toString()}`);
    });

    return () => {
      client.end();
    };
  }, []);

  return (
    <View style={{ flex: 1, justifyContent: 'center', alignItems: 'center' }}>
      <Text>Hello, World!</Text>
    </View>
  );
};

export default App;
