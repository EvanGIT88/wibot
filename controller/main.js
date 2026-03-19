import { Command } from 'commander';
import { sayHello } from './test.js';
import mqtt from "mqtt";

const program = new Command();
const protocol = 'mqtt'
const host = '192.168.0.5'
const port = '1883'
const clientId = `mqtt_${Math.random().toString(16).slice(3)}`

const connectUrl = `${protocol}://${host}:${port}`

const client = mqtt.connect(connectUrl, {
  clientId,
  clean: true,
  connectTimeout: 4000,
  username: '',
  password: '',
  reconnectPeriod: 1000,
})

client.on('connect', () => {
  console.log(`Connected!`)
  console.log(`protocol: ${protocol}`);
    console.log(`host: ${host}`);
      console.log(`port: ${port}`);
        console.log(`clientId: ${clientId}`);
})

program.version("1.0.0")
  .command('say-hello')
 .description('A simple CLI tool to say hello')
  .action(() => {
      sayHello();
    }
  )
 .parse(process.argv);
